#pragma once

#include "esp32-hal-log.h"

namespace xr {
  struct Timer final {
    public:
      explicit Timer(uint32_t amount):
        _interval(amount),
        _remaining(amount),
        _last_time(0)
        {}
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

      const std::pair<Timer, bool> tick(uint32_t time) const noexcept {
        if (time < _last_time) {
          log_e("[warning] - provided a time that is in the past (given %d, last %d)", time, _last_time);

          return std::make_pair(std::move(*this), false);
        }

        if (_last_time == 0) {
          // If this is the first time we've ticked this timer, update the start.
          _last_time = time;

          return std::make_pair(std::move(*this), false);
        }

        // Calculate how much time has passed.
        uint32_t diff = time - _last_time;

        // Update our remaining time.
        _remaining = diff > _remaining ? 0 : _remaining - diff;
        _last_time = time;

        return std::make_pair(std::move(*this), _remaining == 0);
      }

      const bool is_done(void) const {
        return _remaining == 0;
      }

    private:
      mutable uint32_t _interval;
      mutable uint32_t _remaining;
      mutable uint32_t _last_time;
  };
}
