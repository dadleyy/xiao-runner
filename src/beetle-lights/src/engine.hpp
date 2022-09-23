#pragma once

#include <variant>
#include <optional>
#include <vector>
#include <numeric>
#include <array>

namespace beetle_lights {

  using Renderable = std::pair<uint32_t, std::array<uint8_t, 3>>;

  const static uint32_t PLAYER_ATTACK_DURATION = 3000;
  const static uint32_t PLAYER_DEBUFF_DURATION = 3000;

  // The player movement speed is represented here as the amount of milliseconds it takes the
  // player to move a single tile.
  const static uint32_t PLAYER_MOVEMENT_SPEED = 33;

  struct Timer final {
    public:
      explicit Timer(uint32_t amount): _interval(amount), _remaining(amount) {}
      ~Timer() = default;

      Timer(Timer&) = delete;
      Timer& operator=(Timer&) = delete;
      Timer(const Timer&) = delete;
      Timer& operator=(const Timer&) = delete;

      Timer(const Timer&& other):
        _interval(other._interval),
        _remaining(other._remaining),
        _last_time(other._last_time)
        {}

      // @kind MovementAssigment
      const Timer& operator=(const Timer&& other) const noexcept {
        this->_interval = other._interval;
        this->_remaining = other._remaining;
        this->_last_time = other._last_time;
        return *this;
      }

      const std::pair<Timer, bool> tick(uint32_t time) const && noexcept {
        if (_last_time == 0 || time < _last_time || _remaining == 0) {
          // If this is the first time we've ticked this timer, update the start.
          _last_time = _last_time == 0 ? time : _last_time;
          return std::make_pair(std::move(*this), _remaining == 0);
        }

        // Calculate how much time has passed.
        uint32_t diff = time - _last_time;

        // Update our remaining time.
        _remaining = diff > _remaining ? 0 : _remaining - diff;
        _last_time = time;
        return std::make_pair(std::move(*this), _remaining == 0);
      }

    private:
      mutable uint32_t _interval;
      mutable uint32_t _remaining;
      mutable uint32_t _last_time;
  };

  class PlayerState final {
    public:
      enum PlayerStateKind {
        IDLE,
        ATTACKING,
      };

      PlayerState() = default;
      ~PlayerState() = default;

      PlayerState& operator=(const PlayerState&) = delete;
      PlayerState(const PlayerState&) = delete;
      PlayerState& operator=(PlayerState&) = delete;
      PlayerState(PlayerState&) = delete;

      // @kind MovementAssigment
      const PlayerState& operator=(const PlayerState&& other) const noexcept {
        this->_attack_timer = std::move(other._attack_timer);
        this->_idle_timer = std::move(other._idle_timer);
        this->_kind = other._kind;
        return *this;
      }

      PlayerState(const PlayerState&& other):
        _kind(other._kind),
        _attack_timer(std::move(other._attack_timer)),
        _idle_timer(std::move(other._idle_timer))
        {}

      const PlayerState update(
        std::optional<std::pair<uint32_t, uint32_t>> &input,
        uint32_t time
      ) const && noexcept {
        auto [next_idle, has_idled] = std::move(_idle_timer).tick(time);
        _idle_timer = std::move(next_idle);

        // If we have an input message and it is above our threshold and we aren't already attacking,
        // update our state and kick off our action frames.
        if (input != std::nullopt && std::get<1>(*input) > 2000 && _kind == PlayerStateKind::IDLE && has_idled) {
          _kind = PlayerStateKind::ATTACKING;
          _attack_timer = Timer(PLAYER_ATTACK_DURATION);
        }

        if (_kind == PlayerStateKind::ATTACKING) {
          // While we are attacking, continuously reset the idle timer.
          _idle_timer = Timer(PLAYER_DEBUFF_DURATION);

          auto [next, is_done] = std::move(_attack_timer).tick(time);

          if (is_done) {
            _kind = PlayerStateKind::IDLE;
          }

          _attack_timer = is_done ? std::move(next) : Timer(PLAYER_ATTACK_DURATION);
        }

        return std::move(*this);
      }

      const bool is_attacking(void) const {
        return _kind == PlayerStateKind::ATTACKING;
      }

      const std::array<uint8_t, 3> color(void) const {
        if (is_attacking()) {
          return { 0, 255, 0 };
        }

        return { 255, 255, 255 };
      }

    private:
      mutable PlayerStateKind _kind = PlayerStateKind::IDLE;
      const Timer _attack_timer = Timer(PLAYER_ATTACK_DURATION);
      const Timer _idle_timer = Timer(PLAYER_DEBUFF_DURATION);
  };

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
        return std::make_pair(std::move(*this), 0);
      }

      const std::optional<Renderable> renderable(void) const {
        auto color = std::visit(CallColor{}, _kind);
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
      struct CallColor final {
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

      mutable std::variant<Enemy, Slower, Dead> _kind;
      mutable uint32_t _position;
  };

  class Level final {
      public:
        Level() {
          log_d("creating an engine");
          _obstacles.push_back(Obstacle(10));
        };

        Level(Level&) = delete;
        Level& operator=(Level&) = delete;
        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

        Level(const Level&& other):
          _player_position(other._player_position),
          _player_direction(other._player_direction),
          _player_movement_timer(std::move(other._player_movement_timer)),
          _player_state(std::move(other._player_state)) {
            _obstacles.swap(other._obstacles);
          }

        // @kind MovementAssigment
        const Level& operator=(const Level&& other) const noexcept {
          this->_player_position = other._player_position;
          // this->_player_movement_timer = std::move(other._player_movement_timer);
          this->_player_direction = other._player_direction;
          this->_player_state = std::move(other._player_state);

          this->_obstacles.swap(other._obstacles);

          return *this;
        }

        bool is_complete(void) const noexcept {
          return _player_direction == 3;
        }

        // Level update
        //
        // ...
        const std::pair<Level &&, std::vector<Renderable> &&> update(
          std::optional<std::pair<uint32_t, uint32_t>> &input,
          uint32_t time
        ) const && noexcept {
          std::vector<Renderable> lights;
          // Check to see if we can move the player. If so, reset our timer.
          auto [next_player_movement_timer, did_move] = std::move(_player_movement_timer).tick(time);
          _player_movement_timer = did_move ? Timer(PLAYER_MOVEMENT_SPEED) : std::move(next_player_movement_timer);

          // If we moved, calculate the new player position.
          if (did_move) {
            if (_player_direction == 1) {
              _player_position = _player_position >= 100 ? _player_position : _player_position + 1;
            } else if (_player_direction == 2) {
              _player_position = _player_position > 0 ? _player_position - 1 : _player_position;
            }
          }

          // Update the state of the player.
          _player_state = std::move(_player_state).update(input, time);

          // Update the direction of the player if we had a valid input this update.
          if (input != std::nullopt) {
            auto x_input = std::get<0>(*input);

            if (x_input > 2000) {
              _player_direction = 1;
            } else if (x_input < 1000) {
              _player_direction = 2;
            } else {
              _player_direction = 0;
            }
          }

          // Iterate over our obstacles, updating them based on the player position and new state.
          for (auto start = _obstacles.cbegin(); start != _obstacles.cend(); start++) {
            auto [new_state, update] = std::move(*start).update(time, _player_position, _player_state);
            auto renderable = new_state.renderable();

            // Not all obstacles are renderable.
            if (renderable != std::nullopt) {
              lights.push_back(*renderable);
            }

            *start = std::move(new_state);
          }

          Renderable led = std::make_pair(_player_position, _player_state.color());
          lights.push_back(led);
          return std::make_pair(std::move(*this), std::move(lights));
        }

      private:
        mutable uint32_t _player_position = 0;
        mutable uint8_t _player_direction = 0;

        const Timer _player_movement_timer = Timer(PLAYER_MOVEMENT_SPEED);
        const PlayerState _player_state;

        mutable std::vector<Obstacle> _obstacles;
  };

}
