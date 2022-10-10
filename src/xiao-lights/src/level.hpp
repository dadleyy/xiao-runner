#pragma once

#include "timer.hpp"
#include "animation.hpp"
#include "message.hpp"
#include "obstacle.hpp"
#include "player.hpp"

class Level final {
  public:
    enum LevelStateKind {
      IN_PROGRESS,
      FAILED,
      COMPLETE,
    };

    explicit Level(std::pair<const char *, uint32_t> layout, uint32_t bound):
      _data(new std::vector<Light>(0)),
      _player(new Player()),
      _obstacles(new std::vector<Obstacle>(0)),
      _completion_timer(nullptr),
      _state(LevelStateKind::IN_PROGRESS),
      _boundary(bound) {
        _obstacles->reserve(OBSTACLE_BUFFER_SIZE);
        _data->reserve(CONTAINER_BUFFER_SIZE);

        const char * cursor = std::get<0>(layout);
        uint32_t index = 0;

        while (index < _boundary) {
          if (cursor == nullptr || *cursor == '\0' || *cursor == '\n') {
            break;
          }
          auto attempt = Obstacle::try_from(*cursor, index);
          if (attempt != std::nullopt) {
            _obstacles->push_back(std::move(attempt.value()));
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
      _data(std::move(other._data)),
      _player(std::move(other._player)),
      _obstacles(std::move(other._obstacles)),
      _completion_timer(std::move(other._completion_timer)),
      _state(other._state),
      _boundary(other._boundary) {
      }

    const Level& operator=(const Level&& other) const {
      _boundary = other._boundary;
      _state = other._state;
      _data = std::move(other._data);
      _player = std::move(other._player);
      _obstacles = std::move(other._obstacles);
      _completion_timer = std::move(other._completion_timer);
      return *this;
    }

    std::vector<Light>::const_iterator light_begin() const {
      return _data->cbegin();
    }

    std::vector<Light>::const_iterator light_end() const {
      return _data->cend();
    }

    LevelStateKind state() const {
      return !_completion_timer || !_completion_timer->is_done()
        ? LevelStateKind::IN_PROGRESS
        : _state;
    }

    const Level frame(uint32_t current_time, const std::optional<ControllerInput>& input) const && noexcept {
      _data->clear();

      if (_state == LevelStateKind::COMPLETE || _state == LevelStateKind::FAILED) {
        if (!_completion_timer) {
          log_e("no animation time created on level complete");

          return std::move(*this);
        }

        auto [new_timer, is_done] = std::move(*_completion_timer).tick(current_time);
        _completion_timer = std::make_unique<Animation>(std::move(new_timer));

        auto anim_start = _completion_timer->light_begin();
        auto anim_end = _completion_timer->light_end();
        for (auto light = anim_start; light != anim_end; light++) {
          _data->push_back(*light);
        }

        return std::move(*this);
      }

      auto [new_player, message] = std::move(*_player).frame(current_time, input);
      _player = std::make_unique<Player>(std::move(new_player));

      for (auto obstacle = _obstacles->begin(); obstacle != _obstacles->end(); obstacle++) {
        auto [new_obstacle, update] = std::move(*obstacle).frame(current_time, message);
        message = update;

        for (auto light = new_obstacle.light_begin(); light != new_obstacle.light_end(); light++) {
          _data->push_back(*light);
        }

        *obstacle = std::move(new_obstacle);
      }

      for (auto light = _player->light_begin(); light != _player->light_end(); light++) {
        _data->push_back(*light);
      }

      if (std::holds_alternative<GoalReached>(message)) {
        _data->clear();
        _completion_timer = std::make_unique<Animation>(Animation::MiddleOut { _boundary / 2, _boundary, std::make_tuple(0, 255, 0) });

        _state = LevelStateKind::COMPLETE;
      } else if (std::holds_alternative<ObstacleCollision>(message)) {
        _data->clear();
        _completion_timer = std::make_unique<Animation>(Animation::MiddleOut { _boundary / 2, _boundary, std::make_tuple(255, 0, 0) });

        _state = LevelStateKind::FAILED;
      }

      return std::move(*this);
    }

  private:
    struct RunningState final {
      RunningState() {}
      ~RunningState() = default;
      RunningState(const RunningState&) = delete;
      RunningState& operator=(const RunningState&) = delete;
      RunningState(const RunningState&& other) {
      }
      RunningState& operator=(const RunningState&& other) {
        return *this;
      }

      mutable std::unique_ptr<const Player> _player;
      mutable std::unique_ptr<std::vector<Obstacle>> _obstacles;
    };

    struct CompletedState final {
      CompletedState() {}
      ~CompletedState() = default;
      CompletedState(const CompletedState&) = delete;
      CompletedState& operator=(const CompletedState&) = delete;
      CompletedState(const CompletedState&& other) {
      }
      CompletedState& operator=(const CompletedState&& other) {
        return *this;
      }

      mutable std::unique_ptr<Animation> _completion_timer;
    };

    using InnerState = std::variant<RunningState, CompletedState>;

    mutable std::unique_ptr<std::vector<Light>> _data;
    mutable std::unique_ptr<const Player> _player;
    mutable std::unique_ptr<std::vector<Obstacle>> _obstacles;
    mutable std::unique_ptr<Animation> _completion_timer;
    mutable LevelStateKind _state;
    mutable uint32_t _boundary;
};
