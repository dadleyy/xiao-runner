#pragma once

#include <variant>
#include "timer.hpp"
#include "types.hpp"

class Animation final {
  public:
    struct MiddleOut final {
      uint32_t origin;
      uint32_t boundary;
      std::tuple<uint8_t, uint8_t, uint8_t> color;
    };

    using AnimationConfig = std::variant<MiddleOut>;

    explicit Animation(AnimationConfig config):
      _total_timer(new xr::Timer(3000)),
      _tick_timer(new xr::Timer(30)),
      _lights(new std::vector<Light>(0)),
      _frame(0),
      _config(config),
      _done(false) {
        _lights->reserve(100);
      }
    ~Animation() = default;

    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;

    Animation(const Animation&& other):
      _total_timer(std::move(other._total_timer)),
      _tick_timer(std::move(other._tick_timer)),
      _lights(std::move(other._lights)),
      _frame(other._frame),
      _config(other._config),
      _done(other._done) { }

    Animation& operator=(const Animation&& other) {
      _total_timer = std::move(other._total_timer);
      _lights = std::move(other._lights);
      _config = std::move(other._config);
      _frame = other._frame;
      _done = other._done;
      _tick_timer = std::move(other._tick_timer);
      return *this;
    }

    std::vector<Light>::const_iterator light_begin() const {
      return _lights->cbegin();
    }

    std::vector<Light>::const_iterator light_end() const {
      return _lights->cend();
    }

    std::tuple<Animation, bool> tick(uint32_t time) && {
      if (_done) {
        log_d("animation already complete");
        return std::make_tuple(std::move(*this), true);
      }

      auto [new_total, is_done] = std::move(*_total_timer).tick(time);

      if (is_done) {
        log_d("animation has completed");
        _done = true;
        return std::make_tuple(std::move(*this), true);
      }

      auto [new_tick, tick_done] = std::move(*_tick_timer).tick(time);

      _tick_timer = tick_done
        ? std::make_unique<xr::Timer>(30)
        : std::make_unique<xr::Timer>(std::move(new_tick));

      if (tick_done) {
        _lights->clear();
        auto visitor = ConfigVisitor{_lights.get(), _frame};
        std::visit(visitor, _config);
        _frame += 1;
      }

      _total_timer = std::make_unique<xr::Timer>(std::move(new_total));

      return std::make_tuple(std::move(*this), false);
    }

    bool is_done() const {
      return _done;
    }

  private:
    struct ConfigVisitor final {
      ConfigVisitor(std::vector<Light> * const b, uint32_t f): _buffer(b), _frame(f) {}

      void operator()(const MiddleOut& config) {
        auto [red, green, blue] = config.color;
        auto max_capacity_iter = _buffer->capacity() / 2;

        for (uint32_t i = 0; i < _frame && i < max_capacity_iter; i++) {
          if (i + 1 > config.origin || config.origin + i > config.boundary) {
            continue;
          }

          _buffer->push_back(std::make_tuple(config.origin + (i + 1), red, green, blue));
          _buffer->push_back(std::make_tuple(config.origin - (i + 1), red, green, blue));
        }
      }

      std::vector<Light> * const _buffer;
      uint32_t _frame;
    };

    mutable std::unique_ptr<xr::Timer> _total_timer;
    mutable std::unique_ptr<xr::Timer> _tick_timer;
    mutable std::unique_ptr<std::vector<Light>> _lights;
    mutable uint32_t _frame;
    mutable AnimationConfig _config;
    mutable bool _done;
};
