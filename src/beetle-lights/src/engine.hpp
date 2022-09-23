#pragma once

#include <variant>
#include <optional>
#include <vector>
#include <array>

namespace beetle_lights {

  using Renderable = std::pair<uint32_t, std::array<uint8_t, 3>>;

  class PlayerState final {
    public:
      const static uint32_t ACTION_DURATION = 3;
      const static uint32_t ACTION_DEBUFF = 1;

      PlayerState() = default;
      ~PlayerState() = default;

      PlayerState& operator=(PlayerState&) = delete;
      PlayerState(PlayerState&) = delete;

      const PlayerState& operator=(const PlayerState&& other) const {
        this->kind = other.kind;
        this->last_time = other.last_time;
        return *this;
      }

      PlayerState(const PlayerState&& other):
        kind(other.kind),
        last_time(other.last_time) {
      }

      const PlayerState update(
        std::optional<std::pair<uint32_t, uint32_t>> &input,
        uint32_t time
      ) const && noexcept {
        if (input != std::nullopt && std::get<1>(*input) > 2000 && action_frames == 0) {
          kind = 1;
          action_frames = PlayerState::ACTION_DURATION;
        }

        uint32_t time_diff = time - last_time;

        if (time_diff > 1000) {
          last_time = time;
          action_frames = action_frames > 0 ? action_frames - 1 : 0;

          if (action_frames == 0) {
            kind = 0;
          }
        }

        return std::move(*this);
      }

      const bool is_attacking(void) const {
        return kind == 1 && action_frames > PlayerState::ACTION_DEBUFF;
      }

      const std::array<uint8_t, 3> color(void) const {
        if (is_attacking()) {
          return { 0, 255, 0 };
        }

        return { 255, 255, 255 };
      }

    private:
      mutable uint8_t kind = 0;
      mutable uint32_t last_time = 0;
      mutable uint32_t action_frames = 0;
  };

  class Level final {
      public:
        Level() {
          log_d("creating an engine");
          obstacles.push_back(false);
        };

        Level(Level&) = delete;
        Level& operator=(Level&) = delete;
        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

        Level(const Level&& other):
          last_time(other.last_time),
          frame_count(other.frame_count),

          player_position(other.player_position),
          player_direction(other.player_direction),
          player_state(std::move(other.player_state)) {
            obstacles.swap(other.obstacles);
        }

        const Level& operator=(const Level&& other) const noexcept {
          this->last_time = other.last_time;
          this->frame_count = other.frame_count;

          this->player_position = other.player_position;
          this->player_direction = other.player_direction;
          this->player_state = std::move(other.player_state);

          this->obstacles.swap(other.obstacles);

          return *this;
        }

        bool is_complete(void) const noexcept {
          return player_direction == 3;
        }

        // Level update
        //
        // ...
        const std::pair<Level &&, std::vector<Renderable> &&> update(
          std::optional<std::pair<uint32_t, uint32_t>> &input,
          uint32_t time
        ) const && noexcept {
          std::vector<Renderable> lights;
          uint32_t dt = time - last_time;

          // Update the state of the player.
          player_state = std::move(player_state).update(input, time);

          // Update the direction of the player if we had a valid input this update.
          if (input != std::nullopt) {
            auto x_input = std::get<0>(*input);

            if (x_input > 2000) {
              player_direction = 1;
            } else if (x_input < 1000) {
              player_direction = 2;
            } else {
              player_direction = 0;
            }
          }

          for (auto start = obstacles.cbegin(); start != obstacles.cend(); start++) {
          }

          // If we've accumulated enough time for a frame here, actually apply our
          // update to the player position.
          if (dt > 33) {
            if (player_direction == 1) {
              player_position += 1;
            } else if (player_direction == 2) {
              player_position = player_position == 0 ? 0 : player_position - 1;
            }

            frame_count += 1;
            last_time = time;
          }

          Renderable led = std::make_pair(player_position, player_state.color());
          lights.push_back(led);
          return std::make_pair(std::move(*this), std::move(lights));
        }

      private:
        mutable uint32_t last_time = 0;
        mutable uint32_t frame_count = 0;

        mutable uint32_t player_position = 0;
        mutable uint8_t player_direction = 0;
        mutable PlayerState player_state;
        mutable std::vector<bool> obstacles;
  };

}
