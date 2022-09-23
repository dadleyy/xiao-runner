#pragma once

#include <optional>
#include <variant>
#include <vector>
#include "timer.hpp"
#include "direction.hpp"
#include "player.hpp"
#include "interaction.hpp"

namespace beetle_lights {
  class Obstacle final {
    private:
      struct Enemy final {
        Enemy() = default;
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

        mutable Direction _direction = Direction::LEFT;
        mutable uint32_t _position = 0;
        mutable uint32_t _origin = 0;
        const Timer _movement_timer = Timer(1000);
      };

      struct Dead final {
      };

      using ObstacleKinds = std::variant<Enemy, Dead>;

    public:
      explicit Obstacle(uint32_t pos): _kind(Enemy()) {}
      ~Obstacle() {}

      Obstacle(Obstacle&) = delete;
      Obstacle& operator=(Obstacle&) = delete;
      Obstacle(const Obstacle&) = delete;
      Obstacle& operator=(const Obstacle&) = delete;

      Obstacle(const Obstacle&& other): _kind(std::move(other._kind)) {}
      // @kind MovementAssigment
      const Obstacle& operator=(const Obstacle&& other) const noexcept {
        this->_kind = std::move(other._kind);
        return *this;
      }

      const std::tuple<Obstacle, Interaction, std::vector<Renderable>> update(uint32_t time, const PlayerState &player_state) const && noexcept {
        auto [next_kind, updated] = std::visit(UpdateVisitor{time, player_state}, _kind);
        _kind = std::move(next_kind);
        auto outputs = this->renderable();
        return std::make_tuple(std::move(*this), Interaction::NONE, outputs);
      }

      const std::vector<Renderable> renderable(void) const {
        std::vector<Renderable> out;
        return out;
      }

      // A visitor that will return the color for each kind of obstacle.
      struct RenderableVisitor final {
        std::vector<Renderable> operator()(const Enemy& en) {
          std::vector<Renderable> out;
          out.push_back(std::make_pair(en._position, (std::array<uint8_t, 3>) { 255, 0, 0 }));
          return out;
        }

        std::vector<Renderable> operator()(const Dead& dead) {
          std::vector<Renderable> out;
          return out;
        }
      };

      struct UpdateVisitor final {
        UpdateVisitor(uint32_t time, const PlayerState& player_state): _time(time), _player_state(player_state) {}

        const std::pair<ObstacleKinds, bool> operator()(const Enemy& en) const && {
          auto [updated_timer, is_done] = std::move(en._movement_timer).tick(_time);
          en._movement_timer = is_done ? Timer(1000) : std::move(updated_timer);

          if (!is_done) {
            return std::make_pair(std::move(en), false);
          }

          bool cranked_left = en._position + 10 > en._origin;
          bool cranked_right = en._position < en._origin - 10;

          if (cranked_left || cranked_right) {
            en._direction = cranked_left ? Direction::RIGHT : Direction::LEFT;
          }

          en._position = en._direction == Direction::LEFT
            ? en._position + 1
            : en._position - 1;


          return std::make_pair(std::move(en), false);
        }

        const std::pair<ObstacleKinds, bool> operator()(const Dead& en) const && {
          return std::make_pair(std::move(en), false);
        }

        uint32_t _time;
        const PlayerState& _player_state;
      };

      mutable ObstacleKinds _kind;
  };
}
