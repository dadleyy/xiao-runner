#pragma once

#include <optional>
#include <array>
#include <variant>
#include "esp32-hal-log.h"
#include "timer.hpp"
#include "direction.hpp"
#include "message.hpp"

namespace beetle_lights {
  using ObstacleLightBuffer = std::array<std::optional<Renderable>, 20>;

  class Obstacle final {
    private:
      // Declare `UpdateVisitor` early for `friend` access.
      class UpdateVisitor;
      static const uint16_t ENEMY_MS_PER_MOVE = 100;

      struct Enemy final {
        public:
          Enemy() = delete;
          explicit Enemy(uint32_t pos):
            _direction(Direction::LEFT),
            _position(pos),
            _origin(pos),
            _movement_timer(Timer(ENEMY_MS_PER_MOVE))
            {}
          ~Enemy() = default;

          Enemy(const Enemy&) = delete;
          Enemy& operator=(const Enemy&) = delete;

          Enemy(const Enemy&& other):
            _direction(other._direction),
            _position(other._position),
            _origin(other._origin),
            _movement_timer(std::move(other._movement_timer))
            {}

          const Enemy& operator=(const Enemy&& other) noexcept {
            this->_direction = other._direction;
            this->_position = other._position;
            this->_origin = other._origin;
            this->_movement_timer = std::move(other._movement_timer);
            return *this;
          }

        private:
          friend class UpdateVisitor;
          mutable Direction _direction;
          mutable uint32_t _position;
          mutable uint32_t _origin;
          const Timer _movement_timer;
      };

      struct Goal final {
        public:
          Goal() = delete;
          Goal(uint16_t p): position(p) {}
          ~Goal() = default;

          Goal(const Goal&) = delete;
          Goal& operator=(const Goal&) = delete;

          Goal(const Goal&& other): position(other.position) {}
          const Goal& operator=(const Goal&& other) noexcept {
            this->position = other.position;
            return *this;
          }

        private:
          friend class UpdateVisitor;
          mutable uint16_t position;
      };

      struct Corpse final {
        Corpse() = default;
        ~Corpse() = default;

        Corpse(const Corpse&) = delete;
        Corpse& operator=(const Corpse&) = delete;

        Corpse(const Corpse&& other) {}
        const Corpse& operator=(const Corpse&& other) noexcept {
          return *this;
        }
      };

      using ObstacleKinds = std::variant<Enemy, Goal, Corpse>;
     
      struct UpdateVisitor final {
        UpdateVisitor(uint32_t time, ObstacleLightBuffer& lights, Message message):
          _time(time),
          _lights(lights),
          _message(std::move(message)),
          _light_index(0)
          {}

        ~UpdateVisitor() {}

        UpdateVisitor(const UpdateVisitor&) = delete;
        UpdateVisitor& operator=(const UpdateVisitor&) = delete;

        const std::pair<ObstacleKinds, Message> operator()(const Goal& goal) const && {
          _lights[_light_index] = std::make_pair(goal.position, (std::array<uint8_t, 3>) { 255, 255, 0 });

          if (std::holds_alternative<PlayerMovement>(_message) != true) {
            return std::make_pair(std::move(goal), std::move(_message));
          }

          auto player_movement = std::get_if<PlayerMovement>(&_message);

          if (player_movement->position == goal.position) {
            return std::make_pair(std::move(goal), Message { std::in_place_type<GoalReached> });
          }

          return std::make_pair(std::move(goal), std::move(_message));
        }

        const std::pair<ObstacleKinds, Message> operator()(const Enemy& en) const && {
          auto [updated_timer, has_moved] = std::move(en._movement_timer).tick(_time);
          en._movement_timer = has_moved ? Timer(ENEMY_MS_PER_MOVE) : std::move(updated_timer);

          _lights[_light_index] = std::make_pair(en._position, (std::array<uint8_t, 3>) { 255, 0, 0 });
          _light_index += 1;

          auto original_position = en._position;

          if (has_moved) {
            en._position = en._direction == Direction::LEFT
              ? en._position + 1
              : en._position - 1;

            if (en._direction == Direction::LEFT && en._position > (en._origin + 10)) {
              en._direction = Direction::RIGHT;
            } else if (en._direction == Direction::RIGHT && en._position < (en._origin - 10)) {
              en._direction = Direction::LEFT;
            }
          }

          // Do not bother attempting to process non-move related messages
          if (std::holds_alternative<PlayerMovement>(_message) != true) {
            return std::make_pair(std::move(en), std::move(_message));
          }

          auto player_movement = std::get_if<PlayerMovement>(&_message);

          // If we aren't touching this obstacle, ignore.
          if (player_movement->position != original_position) {
            return std::make_pair(std::move(en), std::move(_message));
          }

          if (player_movement->is_attacking == false) {
            log_d("player may be dead, returning collision message");

            return std::make_pair(
              std::move(en),
              Message { std::in_place_type<ObstacleCollision>, en._position }
            );
          }

          // At this point there was a hit and the player wasn't attacking. we are dead.
          return std::make_pair(Corpse(), std::move(_message));
        }

        const std::pair<ObstacleKinds, Message> operator()(const Corpse& corpse) const && {
          return std::make_pair(std::move(corpse), std::move(_message));
        }

        uint32_t _time;
        ObstacleLightBuffer& _lights;
        mutable Message _message;
        mutable uint32_t _light_index;
      };

    public:
      Obstacle(): _kind(Corpse()), _lights() {}
      explicit Obstacle(char symbol, uint32_t pos): _kind(Corpse()), _lights() {
        switch (symbol) {
          case 'x':
            _kind = ObstacleKinds { std::in_place_type<Enemy>, pos };
            break;
          case 'g':
            _kind = ObstacleKinds { std::in_place_type<Goal>, pos };
            break;
        }
        log_d("constructing obstacle");
      }
      ~Obstacle() = default;

      Obstacle(Obstacle&) = delete;
      Obstacle& operator=(Obstacle&) = delete;
      Obstacle(const Obstacle&) = delete;
      Obstacle& operator=(const Obstacle&) = delete;

      Obstacle(const Obstacle&& other):
        _kind(std::move(other._kind)),
        _lights(std::move(other._lights))
        {}

      // @kind MovementAssigment
      const Obstacle& operator=(const Obstacle&& other) const noexcept {
        this->_kind = std::move(other._kind);
        this->_lights = std::move(other._lights);
        return *this;
      }

      ObstacleLightBuffer::const_iterator first_light(void) {
        return _lights.cbegin();
      }

      ObstacleLightBuffer::const_iterator last_light(void) {
        return _lights.cend();
      }

      const std::tuple<Obstacle, Message> update(
        uint32_t time,
        Message&& message
      ) const && noexcept {
        // Clear out our light buffer.
        for (uint32_t i = 0; i < _lights.size(); i++) {
          _lights[i] = std::nullopt;
        }

        // Update our inner kind, providing it the ability to update itself.
        auto [next_kind, obstacle_message] = std::visit(UpdateVisitor{time, _lights, std::move(message)}, _kind);

        _kind = std::move(next_kind);
        return std::make_tuple(std::move(*this), std::move(obstacle_message));
      }

      mutable ObstacleKinds _kind;
      mutable ObstacleLightBuffer _lights;
  };
}
