#pragma once

#include "esp32-hal-log.h"
#include <memory>
#include <vector>
#include <optional>
#include <variant>

#include "timer.hpp"
#include "types.hpp"

class Obstacle final {
  private:
    constexpr static const uint32_t OBJECT_BUFFER_SIZE = 30;
    static const uint16_t ENEMY_MS_PER_MOVE = 100;
    static const uint16_t SNAKE_MS_PER_MOVE = 1000;
    static const uint16_t SNAKE_EYE_SIZE_HALF = 5;
    static const uint16_t SNAKE_WINGS_SIZE_HALF = 12;

    constexpr static const std::tuple<uint8_t, uint8_t, uint8_t> SNAKE_COLOR = std::make_tuple(255, 100, 0);
    constexpr static const std::tuple<uint8_t, uint8_t, uint8_t> PAWN_COLOR = std::make_tuple(255, 20, 0);
    constexpr static const std::tuple<uint8_t, uint8_t, uint8_t> GOAL_COLOR = std::make_tuple(100, 150, 0);

    class FrameVisitor;

    struct Snake final {
      public:
        Snake() = delete;
        explicit Snake(uint32_t pos):
          _direction(Direction::LEFT),
          _position(pos),
          _origin(pos),
          _movement_timer(xr::Timer(ENEMY_MS_PER_MOVE))
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
        friend class FrameVisitor;
        mutable Direction _direction;
        mutable uint32_t _position;
        mutable uint32_t _origin;
        const xr::Timer _movement_timer;
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

    struct Pawn final {
      public:
        Pawn() = delete;
        explicit Pawn(uint32_t pos):
          _direction(Direction::LEFT),
          _position(pos),
          _origin(pos),
          _movement_timer(xr::Timer(ENEMY_MS_PER_MOVE))
          {}
        ~Pawn() = default;

        Pawn(const Pawn&) = delete;
        Pawn& operator=(const Pawn&) = delete;

        Pawn(const Pawn&& other):
          _direction(other._direction),
          _position(other._position),
          _origin(other._origin),
          _movement_timer(std::move(other._movement_timer))
          {}

        const Pawn& operator=(const Pawn&& other) noexcept {
          this->_direction = other._direction;
          this->_position = other._position;
          this->_origin = other._origin;
          this->_movement_timer = std::move(other._movement_timer);
          return *this;
        }

      private:
        friend class FrameVisitor;
        mutable Direction _direction;
        mutable uint32_t _position;
        mutable uint32_t _origin;
        const xr::Timer _movement_timer;
    };

    struct Goal final {
      public:
        Goal() = delete;
        Goal(uint16_t p): _position(p) {}
        ~Goal() = default;

        Goal(const Goal&) = delete;
        Goal& operator=(const Goal&) = delete;

        Goal(const Goal&& other): _position(other._position) {}
        const Goal& operator=(const Goal&& other) noexcept {
          _position = other._position;
          return *this;
        }

      private:
        friend class FrameVisitor;
        mutable uint16_t _position;
    };

    using ObstacleKind = std::variant<Pawn, Snake, Goal, Corpse>;

  public:
    Obstacle(): Obstacle(Pawn(0)) {}
    ~Obstacle() = default;

    static std::optional<Obstacle> try_from(char token, uint32_t location) {
      switch (token) {
        case 'x':
          log_d("creating pawn at %d", location);
          return Obstacle { Pawn(location) };
        case 'g':
          log_d("creating goal at %d", location);
          return Obstacle { Goal(location) };
        case 's':
          log_d("creating snake at %d", location);
          return Obstacle { Snake(location) };
        default:
          return std::nullopt;
      }
    }

    Obstacle(const Obstacle&) = delete;
    Obstacle& operator=(const Obstacle&) = delete;

    Obstacle(const Obstacle&& other):
      _data(std::move(other._data)),
      _kind(std::move(other._kind)) {
    }

    Obstacle& operator=(const Obstacle&& other) {
      _data = std::move(other._data);
      _kind = std::move(other._kind);
      return *this;
    }

    std::vector<Light>::const_iterator light_begin() const {
      return _data->cbegin();
    }

    std::vector<Light>::const_iterator light_end() const {
      return _data->cend();
    }

    std::tuple<const Obstacle, FrameMessage> frame(uint32_t time, const FrameMessage& input) const && {
      _data->clear();
      auto visitor = FrameVisitor { time, input, _data.get() };
      auto [new_kind, message] = std::visit(visitor, std::move(_kind));
      _kind = std::move(new_kind);
      return std::make_tuple(std::move(*this), message);
    }

  private:
    class FrameVisitor final {
      public:
        explicit FrameVisitor(
          uint32_t time,
          const FrameMessage& input,
          std::vector<Light> * const data
        ): _time(time), _input(input), _data(data) {
        }

        std::tuple<ObstacleKind, FrameMessage> operator()(const Snake& snake) const {
          auto [updated_timer, has_moved] = std::move(snake._movement_timer).tick(_time);
          snake._movement_timer = has_moved
            ? xr::Timer(SNAKE_MS_PER_MOVE)
            : std::move(updated_timer);

          auto new_position = has_moved
            ? snake._direction == Direction::LEFT ? snake._position + 1 : snake._position - 1
            : snake._position;

          if (snake._position + SNAKE_EYE_SIZE_HALF > snake._origin) {
            snake._direction = Direction::RIGHT;
          } else if (snake._position > SNAKE_EYE_SIZE_HALF && snake._position - SNAKE_EYE_SIZE_HALF < snake._origin) {
            snake._direction = Direction::LEFT;
          }

          FrameMessage result = std::move(_input);

          for (uint32_t i = 0; i < (SNAKE_WINGS_SIZE_HALF + SNAKE_WINGS_SIZE_HALF); i++) {
            uint32_t light_position = 0;

            if (i < SNAKE_WINGS_SIZE_HALF) {
              light_position = snake._position + i + SNAKE_EYE_SIZE_HALF;
            } else {
              if (snake._position < (i + SNAKE_EYE_SIZE_HALF)) {
                continue;
              }

              light_position = snake._position - (i + SNAKE_EYE_SIZE_HALF);
            }

            if (std::holds_alternative<PlayerMovement>(result)) {
              auto player_movement = std::get_if<PlayerMovement>(&result);
              auto did_collide = player_movement->position == light_position;

              if (did_collide && player_movement->attacking == false) {
                result = ObstacleCollision { light_position };
              }
            }

            auto [red, green, blue] = SNAKE_COLOR;
            _data->push_back(std::make_tuple(light_position, red, green, blue));
          }

          snake._position = new_position;

          return std::make_tuple(std::move(snake), std::move(result));
        }

        std::tuple<ObstacleKind, FrameMessage> operator()(const Pawn& pawn) const {
          auto [updated_timer, has_moved] = std::move(pawn._movement_timer).tick(_time);
          pawn._movement_timer = has_moved ? xr::Timer(ENEMY_MS_PER_MOVE) : std::move(updated_timer);

          if (std::holds_alternative<PlayerMovement>(_input)) {
            auto player_movement = std::get_if<PlayerMovement>(&_input);
            bool did_collide = player_movement->position == pawn._position;

            if (did_collide) {
              return player_movement->attacking
                ? std::make_tuple<ObstacleKind>(Corpse(), _input)
                : std::make_tuple<ObstacleKind>(std::move(pawn), ObstacleCollision { pawn._position });
            }
          }

          if (has_moved) {
            pawn._position = pawn._direction == Direction::LEFT
              ? pawn._position + 1
              : pawn._position - 1;

            if (pawn._direction == Direction::LEFT && pawn._position > (pawn._origin + 10)) {
              pawn._direction = Direction::RIGHT;
            } else if (pawn._direction == Direction::RIGHT && pawn._position < (pawn._origin - 10)) {
              pawn._direction = Direction::LEFT;
            }
          }

          auto [red, green, blue] = PAWN_COLOR;
          _data->push_back(std::make_tuple(pawn._position, red, green, blue));

          return std::make_pair(std::move(pawn), _input);
        }

        std::tuple<ObstacleKind, FrameMessage> operator()(const Goal& goal) const {
          auto [red, green, blue] = GOAL_COLOR;
          _data->push_back(std::make_tuple(goal._position, red, green, blue));

          if (std::holds_alternative<PlayerMovement>(_input)) {
            auto player_movement = std::get_if<PlayerMovement>(&_input);
            bool did_collide = player_movement->position == goal._position;

            if (did_collide) {
              return std::make_tuple<ObstacleKind>(std::move(goal), GoalReached { });
            }
          }

          return std::make_tuple(std::move(goal), _input);
        }

        std::tuple<ObstacleKind, FrameMessage> operator()(const Corpse& corpse) const {
          return std::make_tuple(std::move(corpse), _input);
        }

      private:
        uint32_t _time;
        const FrameMessage& _input;
        std::vector<Light> * const _data;
    };

    explicit Obstacle(ObstacleKind&& kind):
      _data(new std::vector<Light>(0)),
      _kind(std::move(kind)) {
      _data->reserve(OBJECT_BUFFER_SIZE);
    }

    mutable std::unique_ptr<std::vector<Light>> _data;
    mutable ObstacleKind _kind;
};
