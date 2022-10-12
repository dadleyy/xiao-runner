#pragma once

#include "timer.hpp"
#include "types.hpp"
#include "animation.hpp"
#include "obstacle.hpp"
#include "player.hpp"

class Level final {
  public:
    constexpr static const uint32_t LEVEL_BUFFER_SIZE = 256;
    constexpr static const uint32_t OBSTACLE_BUFFER_SIZE = 15;

    enum LevelStateKind {
      IN_PROGRESS,
      FAILED,
      COMPLETE,
    };

    explicit Level(std::pair<const char *, uint32_t> layout, uint32_t bound):
      _impl(RunningState()),
      _data(new std::vector<Light>(0)),
      _state(LevelStateKind::IN_PROGRESS),
      _boundary(bound) {
        _data->reserve(LEVEL_BUFFER_SIZE);

        const char * cursor = std::get<0>(layout);
        uint32_t index = 0;

        while (index < _boundary) {
          if (cursor == nullptr || *cursor == '\0' || *cursor == '\n') {
            break;
          }
          auto attempt = Obstacle::try_from(*cursor, index);
          if (attempt != std::nullopt) {
            auto running = std::get_if<RunningState>(&_impl);
            running->_obstacles->push_back(std::move(attempt.value()));
          }
          index++;
          cursor++;
        }
      }

    Level(): Level(std::make_pair("", 0), 0) {}

    ~Level() = default;
    Level(const Level&) = delete;
    Level& operator=(const Level&) = delete;

    Level(const Level&& other):
      _impl(std::move(other._impl)),
      _data(std::move(other._data)),
      _state(other._state),
      _boundary(other._boundary) {
      }

    const Level& operator=(const Level&& other) const {
      _impl = std::move(other._impl);
      _boundary = other._boundary;
      _state = other._state;
      _data = std::move(other._data);
      return *this;
    }

    std::vector<Light>::const_iterator light_begin() const {
      return _data->cbegin();
    }

    std::vector<Light>::const_iterator light_end() const {
      return _data->cend();
    }

    LevelStateKind state() const {
      return LevelStateKind::IN_PROGRESS;
    }

    const Level frame(uint32_t current_time, const std::optional<ControllerInput>& input) const && noexcept {
      _data->clear();
      auto new_state = std::visit(StateVisitor{ _data.get(), current_time, input, _boundary }, _impl);
      _impl = std::move(new_state);

      return std::move(*this);
    }

  private:
    struct RunningState final {
      RunningState(): _player(new Player()), _obstacles(new std::vector<Obstacle>(0)) {
        _obstacles->reserve(OBSTACLE_BUFFER_SIZE);
      }
      ~RunningState() = default;
      RunningState(const RunningState&) = delete;
      RunningState& operator=(const RunningState&) = delete;
      RunningState(const RunningState&& other):
        _player(std::move(other._player)),
        _obstacles(std::move(other._obstacles)) {
        }

      RunningState& operator=(const RunningState&& other) {
        _obstacles = std::move(other._obstacles);
        _player = std::move(other._player);
        return *this;
      }

      mutable std::unique_ptr<const Player> _player;
      mutable std::unique_ptr<std::vector<Obstacle>> _obstacles;
    };

    struct CompletedState final {
      CompletedState(uint32_t boundary):
        _completion_timer(new Animation(Animation::MiddleOut { boundary / 2, boundary, std::make_tuple(255, 0, 0) })) {
        }
      ~CompletedState() = default;
      CompletedState(const CompletedState&) = delete;
      CompletedState& operator=(const CompletedState&) = delete;
      CompletedState(const CompletedState&& other): _completion_timer(std::move(other._completion_timer)) {
      }
      CompletedState& operator=(const CompletedState&& other) {
        _completion_timer = std::move(other._completion_timer);
        return *this;
      }

      mutable std::unique_ptr<Animation> _completion_timer;
    };

    using InnerState = std::variant<RunningState, CompletedState>;

    struct StateVisitor final {
      std::vector<Light> * light_buffer;
      uint32_t current_time;
      const std::optional<ControllerInput>& input;
      uint32_t boundary;

      InnerState operator()(const RunningState& running) {
        auto [new_player, message] = std::move(*running._player).frame(current_time, input);
        running._player = std::make_unique<Player>(std::move(new_player));

        for (auto obstacle = running._obstacles->begin(); obstacle != running._obstacles->end(); obstacle++) {
          auto [new_obstacle, update] = std::move(*obstacle).frame(current_time, message);
          message = update;

          for (auto light = new_obstacle.light_begin(); light != new_obstacle.light_end(); light++) {
            light_buffer->push_back(*light);
          }

          *obstacle = std::move(new_obstacle);
        }

        for (auto light = running._player->light_begin(); light != running._player->light_end(); light++) {
          light_buffer->push_back(*light);
        }

        if (std::holds_alternative<GoalReached>(message)) {
          light_buffer->clear();

          return CompletedState(boundary);
        } else if (std::holds_alternative<ObstacleCollision>(message)) {
          light_buffer->clear();

          return CompletedState(boundary);
        }

        return std::move(running);
      }

      InnerState operator()(const CompletedState& completed) {
        return std::move(completed);
      }
    };


    mutable InnerState _impl;
    mutable std::unique_ptr<std::vector<Light>> _data;
    mutable LevelStateKind _state;
    mutable uint32_t _boundary;
};
