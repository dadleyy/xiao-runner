#pragma once

#include <optional>
#include "timer.hpp"
#include "renderable.hpp"

namespace beetle_lights {
  using AnimationLightBuffer = std::array<std::optional<Renderable>, 50>;

  class Animation final {
    public:
      explicit Animation(bool good):
        _stepper(10),
        _total(3000),
        _is_done(false),
        _origin(50),
        _step(0),
        _light_buffer(std::make_unique<AnimationLightBuffer>()),
        _good(good)
        {}

      Animation(): Animation(false) {}

      Animation(const Animation&) = delete;
      Animation& operator=(const Animation&) = delete;

      Animation(const Animation&& other):
        _stepper(std::move(other._stepper)),
        _total(std::move(other._total)),
        _is_done(other._is_done),
        _origin(other._origin),
        _step(other._step),
        _light_buffer(std::move(other._light_buffer)),
        _good(other._good)
        {}

      const Animation& operator=(const Animation&& other) const {
        this->_stepper = std::move(other._stepper);
        this->_total = std::move(other._total);
        this->_is_done = other._is_done;
        this->_light_buffer = std::move(other._light_buffer);
        this->_step = other._step;
        return *this;
      }

      AnimationLightBuffer::const_iterator lights_start() const {
        return _light_buffer->cbegin();
      }

      AnimationLightBuffer::const_iterator lights_end() const {
        return _light_buffer->cend();
      }

      const bool is_done() const {
        return _is_done;
      }

      const Animation && tick(uint32_t now) const noexcept {
        // _light_buffer->fill(std::nullopt);

        if (_is_done) {
          return std::move(*this);
        }

        auto [new_step, step_done] = std::move(_stepper).tick(now);
        _stepper = std::move(new_step);

        // TODO: this whole computation stinks; this is just a placeholder for now to
        // be able to show _some_ kind of time-based light sequence.
        if (step_done) {
          _step += 1;
          _stepper = Timer(10);
          uint8_t c = 0, j = 0, p = 0;
          std::array<uint8_t, 3> color { 255, 0, 0 };

          if (_good) {
            color = (std::array<uint8_t, 3>) { 0, 255, 0 };
          }

          for (uint32_t i = 0; i < _step; i++) {
            if (c == 10 && j < 50) {
              c = 0;
              (*_light_buffer)[j] = std::make_pair(_origin + p, color);
              (*_light_buffer)[j + 1] = std::make_pair(_origin - p, color);
              p ++;
              j += 2;
            }

            c++;
          }
        }

        auto [new_total, is_all_done] = std::move(_total).tick(now);

        if (is_all_done) {
          _is_done = true;
          return std::move(*this);
        }

        _total = std::move(new_total);

        return std::move(*this);
      }

    private:
      Timer _stepper;
      Timer _total;
      mutable bool _is_done;
      const uint8_t _origin;
      mutable uint32_t _step;
      mutable std::unique_ptr<AnimationLightBuffer> _light_buffer;
      const bool _good;
  };
}
