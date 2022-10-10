#pragma once

#include <optional>
#include <array>
#include <variant>
#include "esp32-hal-log.h"
#include "timer.hpp"
#include "direction.hpp"
#include "message.hpp"

namespace beetle_lights {
  using ObstacleLightBuffer = std::array<std::optional<Renderable>, 100>;

  class Obstacle final {
    private:
      // Declare `UpdateVisitor` early for `friend` access.
      class UpdateVisitor;
      static const uint16_t ENEMY_MS_PER_MOVE = 100;
      static const uint16_t ENEMY_WANDER_DISTANCE_MAX = 15;

      static const uint16_t SNAKE_MS_PER_MOVE = 1000;
      static const uint16_t SNAKE_EYE_SIZE_HALF = 5;
      static const uint16_t SNAKE_WINGS_SIZE_HALF = 12;

      static constexpr const std::array<uint8_t, 3> SNAKE_COLOR = { 255, 120, 0 };
      static constexpr const std::array<uint8_t, 3> GOAL_COLOR = { 200, 230, 0 };
      static constexpr const std::array<uint8_t, 3> ENEMY_COLOR = { 255, 0, 0 };

      struct Snake final {
        public:
          Snake() = delete;
          explicit Snake(uint32_t pos):
            _direction(Direction::LEFT),
            _position(pos),
            _origin(pos),
            _movement_timer(Timer(SNAKE_MS_PER_MOVE))
            {}
          ~Snake() = default;

          Snake(const Snake&) = delete;
          Snake& operator=(const Snake&) = delete;

          Snake(const Snake&& other):
            _direction(other._direction),
            _position(other._position),
            _origin(other._origin),
            _movement_timer(std::move(other._movement_timer))
            {}

          const Snake& operator=(const Snake&& other) noexcept {
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

      using ObstacleKinds = std::variant<Enemy, Snake, Goal, Corpse>;
     
      struct UpdateVisitor final {
        UpdateVisitor(uint32_t time, uint32_t boundary, ObstacleLightBuffer& lights, Message message):
          _time(time),
          _boundary(boundary),
          _lights(lights),
          _message(std::move(message)),
          _light_index(0)
          {}

        ~UpdateVisitor() = default;

        UpdateVisitor(const UpdateVisitor&) = delete;
        UpdateVisitor& operator=(const UpdateVisitor&) = delete;

        // Goal update - check for success.
        const std::pair<ObstacleKinds, Message> operator()(const Goal& goal) const && {
          _lights[_light_index] = std::make_pair(goal.position, GOAL_COLOR);

          if (std::holds_alternative<PlayerMovement>(_message) != true) {
            return std::make_pair(std::move(goal), std::move(_message));
          }

          auto player_movement = std::get_if<PlayerMovement>(&_message);

          if (player_movement->position == goal.position) {
            return std::make_pair(std::move(goal), Message { std::in_place_type<GoalReached> });
          }

          return std::make_pair(std::move(goal), std::move(_message));
        }

        // Snake update
        const std::pair<ObstacleKinds, Message> operator()(const Snake& snake) const && {
          auto [updated_timer, has_moved] = std::move(snake._movement_timer).tick(_time);
          snake._movement_timer = has_moved ? Timer(SNAKE_MS_PER_MOVE) : std::move(updated_timer);

          auto new_position = has_moved
            ? snake._direction == Direction::LEFT ? snake._position + 1 : snake._position - 1
            : snake._position;

          if (snake._position + SNAKE_EYE_SIZE_HALF > snake._origin) {
            snake._direction = Direction::RIGHT;
          } else if (snake._position > SNAKE_EYE_SIZE_HALF && snake._position - SNAKE_EYE_SIZE_HALF < snake._origin) {
            snake._direction = Direction::LEFT;
          }

          Message result = std::move(_message);

          for (uint8_t i = 0; i < (SNAKE_WINGS_SIZE_HALF + SNAKE_WINGS_SIZE_HALF); i++) {
            auto light_position = 0;

            if (i < SNAKE_WINGS_SIZE_HALF) {
              light_position = snake._position + i + SNAKE_EYE_SIZE_HALF;
            } else {
              if (snake._position < (i + SNAKE_EYE_SIZE_HALF)) {
                continue;
              }

              light_position = snake._position - (i + SNAKE_EYE_SIZE_HALF);
            }

            if (std::holds_alternative<PlayerMovement>(_message)) {
              auto player_movement = std::get_if<PlayerMovement>(&_message);
              auto did_collide = player_movement->position == light_position;

              if (did_collide && player_movement->is_attacking == false) {
                result = Message { std::in_place_type<ObstacleCollision>, light_position };
              }
            }

            _lights[_light_index] = std::make_pair(light_position, SNAKE_COLOR);
            _light_index += 1;
          }

          snake._position = new_position;

          return std::make_pair(std::move(snake), std::move(result));
        }

        // Enemy update
        const std::pair<ObstacleKinds, Message> operator()(const Enemy& en) const && {
          auto [updated_timer, has_moved] = std::move(en._movement_timer).tick(_time);
          en._movement_timer = has_moved ? Timer(ENEMY_MS_PER_MOVE) : std::move(updated_timer);

          auto original_position = en._position;

          en._position = has_moved
            ? en._direction == Direction::LEFT ? en._position + 1 : en._position - 1
            : en._position;

          _lights[_light_index] = std::make_pair(en._position, ENEMY_COLOR);
          _light_index += 1;

          // Make sure our origin minus the wander distance is at least 0.
          auto max_start = en._origin >= ENEMY_WANDER_DISTANCE_MAX
            ? en._origin - ENEMY_WANDER_DISTANCE_MAX
            : 0;

          if (en._position >= (en._origin + ENEMY_WANDER_DISTANCE_MAX)) {
            en._direction = Direction::RIGHT;
          } else if (en._position <= max_start) {
            en._direction = Direction::LEFT;
          }

          // Do not bother attempting to process non-move related messages
          if (std::holds_alternative<PlayerMovement>(_message) != true) {
            return std::make_pair(std::move(en), std::move(_message));
          }

          auto player_movement = std::get_if<PlayerMovement>(&_message);

          // If we aren't touching this obstacle we don't need to continue; we're only interested
          // in collisions beyond this point.
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

        // Corpse update - no-op.
        const std::pair<ObstacleKinds, Message> operator()(const Corpse& corpse) const && {
          return std::make_pair(std::move(corpse), std::move(_message));
        }

        uint32_t _time;
        uint32_t _boundary;
        ObstacleLightBuffer& _lights;
        mutable Message _message;
        mutable uint32_t _light_index;
      };

    public:
      static std::optional<std::unique_ptr<Obstacle>> try_from(char symbol, uint32_t pos) {
        switch (symbol) {
          case 's':
            log_d("constructing enemy");

            return std::make_unique<Obstacle>(ObstacleKinds { std::in_place_type<Snake>, pos });
          case 'x':
            log_d("constructing enemy");

            return std::make_unique<Obstacle>(ObstacleKinds { std::in_place_type<Enemy>, pos });
          case 'g':
            log_d("constructing goal");

            return std::make_unique<Obstacle>(ObstacleKinds { std::in_place_type<Goal>, pos });
        }

        return std::nullopt;
      }

      Obstacle(): _kind(Corpse()), _lights() {}
      explicit Obstacle(ObstacleKinds kind): _kind(std::move(kind)), _lights() {}

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

      ObstacleLightBuffer::const_iterator first_light(void) const {
        return _lights.cbegin();
      }

      ObstacleLightBuffer::const_iterator last_light(void) const {
        return _lights.cend();
      }

      // The main obstacle update function is responsible for creating a visitor with the correct state,
      // where the visitor itself is what is ultimately responsible for handling the "pattern matching"
      // on each enemy type.
      const std::tuple<const Obstacle, Message> update(
        uint32_t time,
        Message&& message,
        uint32_t boundary
      ) const && noexcept {
        _lights.fill(std::nullopt);
        // Update our inner kind, providing it the ability to update itself.
        auto [next_kind, obstacle_message] = std::visit(UpdateVisitor{time, boundary, _lights, std::move(message)}, _kind);
        _kind = std::move(next_kind);
        return std::make_tuple(std::move(*this), std::move(obstacle_message));
      }

      mutable ObstacleKinds _kind;
      mutable ObstacleLightBuffer _lights;
  };
}
