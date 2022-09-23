#pragma once

#include <optional>
#include <variant>
#include <vector>
#include "timer.hpp"
#include "player.hpp"

namespace beetle_lights {
  class Obstacle final {
    public:
      explicit Obstacle(uint32_t pos): _position(pos) {}
      ~Obstacle() {}

      Obstacle(Obstacle&) = delete;
      Obstacle& operator=(Obstacle&) = delete;
      Obstacle(const Obstacle&) = delete;
      Obstacle& operator=(const Obstacle&) = delete;

      Obstacle(const Obstacle&& other):
        _kind(std::move(other._kind)),
        _position(other._position)
        {}

      // @kind MovementAssigment
      const Obstacle& operator=(const Obstacle&& other) const noexcept {
        this->_kind = std::move(other._kind);
        this->_position = other._position;
        return *this;
      }

      const std::pair<Obstacle, uint8_t> update(
        uint32_t time,
        uint32_t player_position,
        const PlayerState &player_state
      ) const && noexcept {
        auto [next_kind, updated] = std::visit(UpdateVisitor{time, player_position, player_state}, _kind);
        _kind = std::move(next_kind);
        return std::make_pair(std::move(*this), 0);
      }

      const std::optional<Renderable> renderable(void) const {
        auto color = std::visit(RenderableVisitor{}, _kind);
        if (color == std::nullopt) {
          return std::nullopt;
        }
        return std::make_pair(_position, *color);
      }

    private:
      struct Enemy final {
      };

      struct Slower final {
      };

      struct Dead final {
      };

      // A visitor that will return the color for each kind of obstacle.
      struct RenderableVisitor final {
        std::optional<std::array<uint8_t, 3>> operator()(const Enemy& en) {
          return std::nullopt;
        }

        std::optional<std::array<uint8_t, 3>> operator()(const Slower& slow) {
          return std::nullopt;
        }

        std::optional<std::array<uint8_t, 3>> operator()(const Dead& dead) {
          return std::nullopt;
        }
      };

      struct UpdateVisitor final {
        UpdateVisitor(uint32_t time, uint32_t player_position, const PlayerState& player_state) {}
        const std::pair<std::variant<Enemy, Slower, Dead>, bool> operator()(const Enemy& en) const && {
          return std::make_pair(std::move(en), false);
        }
        const std::pair<std::variant<Enemy, Slower, Dead>, bool> operator()(const Slower& en) const && {
          return std::make_pair(std::move(en), false);
        }
        const std::pair<std::variant<Enemy, Slower, Dead>, bool> operator()(const Dead& en) const && {
          return std::make_pair(std::move(en), false);
        }
      };

      mutable std::variant<Enemy, Slower, Dead> _kind;
      mutable uint32_t _position;
  };
}
