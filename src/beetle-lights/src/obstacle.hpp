#pragma once

#include <optional>
#include <array>
#include <variant>
#include "esp32-hal-log.h"
#include "timer.hpp"
#include "direction.hpp"
#include "player.hpp"
#include "interaction.hpp"

namespace beetle_lights {
  using ObstacleLightBuffer = std::array<std::optional<Renderable>, 20>;

  class Obstacle final {
    public:
      static const uint16_t ENEMY_MS_PER_MOVE = 100;

    private:
      struct Enemy final {
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

        mutable Direction _direction;
        mutable uint32_t _position;
        mutable uint32_t _origin;
        const Timer _movement_timer;
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
      using ObstacleKinds = std::variant<Enemy, Corpse>;

    public:
      Obstacle(): _kind(Corpse()), _lights() {}
      explicit Obstacle(uint32_t pos): _kind(Enemy(pos)),  _lights() {
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

      ObstacleLightBuffer::const_iterator cbegin(void) {
        return _lights.cbegin();
      }

      ObstacleLightBuffer::const_iterator cend(void) {
        return _lights.cend();
      }

      const std::tuple<Obstacle, Interaction> update(
        uint32_t time,
        const PlayerState &player_state
      ) const && noexcept {
        // Clear out our light buffer.
        for (uint32_t i = 0; i < _lights.size(); i++) {
          _lights[i] = std::nullopt;
        }

        // Update our inner kind, providing it the ability to update itself.
        auto [next_kind, updated] = std::visit(UpdateVisitor{time, player_state, _lights}, _kind);

        _kind = std::move(next_kind);
        return std::make_tuple(std::move(*this), Interaction::NONE);
      }
     
      struct UpdateVisitor final {
        UpdateVisitor(uint32_t time, const PlayerState& player_state, ObstacleLightBuffer& _lights):
          _time(time),
          _player_state(player_state),
          _lights(_lights),
          _light_index(0)
          {}

        ~UpdateVisitor() {}

        UpdateVisitor(const UpdateVisitor&) = delete;
        UpdateVisitor& operator=(const UpdateVisitor&) = delete;

        const std::pair<ObstacleKinds, bool> operator()(const Enemy& en) const && {
          auto [updated_timer, is_done] = std::move(en._movement_timer).tick(_time);
          en._movement_timer = is_done ? Timer(ENEMY_MS_PER_MOVE) : std::move(updated_timer);

          _lights[_light_index] = std::make_pair(en._position, (std::array<uint8_t, 3>) { 255, 0, 0 });
          _light_index += 1;

          if (!is_done) {
            return std::make_pair(std::move(en), false);
          }

          en._position = en._direction == Direction::LEFT
            ? en._position + 1
            : en._position - 1;

          if (en._direction == Direction::LEFT && en._position > (en._origin + 10)) {
            en._direction = Direction::RIGHT;
          } else if (en._direction == Direction::RIGHT && en._position < (en._origin - 10)) {
            en._direction = Direction::LEFT;
          }

          return std::make_pair(std::move(en), false);
        }

        const std::pair<ObstacleKinds, bool> operator()(const Corpse& corpse) const && {
          return std::make_pair(std::move(corpse), false);
        }

        uint32_t _time;
        const PlayerState& _player_state;
        ObstacleLightBuffer& _lights;
        mutable uint32_t _light_index;
      };

      mutable ObstacleKinds _kind;
      mutable ObstacleLightBuffer _lights;
  };
}
