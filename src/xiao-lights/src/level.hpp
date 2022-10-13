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
      _boundary(other._boundary) {
      }

    const Level& operator=(const Level&& other) const {
      _impl = std::move(other._impl);
      _boundary = other._boundary;
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
      if (std::holds_alternative<CompletedState>(_impl) != true) {
        return LevelStateKind::IN_PROGRESS;
      }

      auto completed = std::get_if<CompletedState>(&_impl);

      if (completed->_completion_timer->is_done() != true) {
        return LevelStateKind::IN_PROGRESS;
      }

      return completed->_result ? LevelStateKind::COMPLETE : LevelStateKind::FAILED;
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
      CompletedState(bool success, uint32_t boundary):
        _completion_timer(new Animation(Animation::MiddleOut {
          boundary / 2, boundary,
          success ? std::make_tuple(0, 255, 0) : std::make_tuple(255, 0, 0)
        })),
        _result(success) {
        }
      ~CompletedState() = default;
      CompletedState(const CompletedState&) = delete;
      CompletedState& operator=(const CompletedState&) = delete;
      CompletedState(const CompletedState&& other):
        _completion_timer(std::move(other._completion_timer)),
        _result(other._result) {
      }
      CompletedState& operator=(const CompletedState&& other) {
        _result = other._result;
        _completion_timer = std::move(other._completion_timer);
        return *this;
      }

      mutable std::unique_ptr<Animation> _completion_timer;
      mutable bool _result;
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

          return CompletedState(true, boundary);
        } else if (std::holds_alternative<ObstacleCollision>(message)) {
          light_buffer->clear();

          return CompletedState(false, boundary);
        }

        return std::move(running);
      }

      InnerState operator()(const CompletedState& completed) {
        auto [new_timer, is_done] = std::move(*completed._completion_timer).tick(current_time);
        completed._completion_timer = std::make_unique<Animation>(std::move(new_timer));

        auto anim_start = completed._completion_timer->light_begin();
        auto anim_end = completed._completion_timer->light_end();
        for (auto light = anim_start; light != anim_end; light++) {
          light_buffer->push_back(*light);
        }

        return std::move(completed);
      }
    };


    mutable InnerState _impl;
    mutable std::unique_ptr<std::vector<Light>> _data;
    mutable uint32_t _boundary;
};
