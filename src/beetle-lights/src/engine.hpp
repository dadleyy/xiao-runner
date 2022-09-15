#ifndef _ENGINE_H
#define _ENGINE_H 1

#include <variant>
#include <optional>

using Renderable = std::pair<uint32_t, std::array<uint32_t, 3>>;

struct Player final {
  public:
    Player() = default;
    Player(Player&) = delete;
    Player& operator=(Player&) = delete;

    Player(Player&&) = default;
    Player& operator=(Player&&) = default;

  private:
    struct Living final {
    };

    struct Dying final {
    };

    struct Attacking final {
    };

    using PlayerState = std::variant<Living, Dying, Attacking>;
    PlayerState state;
};

struct Engine final {
  Engine() { log_d("creating an engine"); };

  Engine(Engine&) = delete;
  Engine& operator=(Engine&) = delete;
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  Engine(Engine&&) = default;
  Engine& operator=(Engine&&) = default;

  std::pair<Engine, std::vector<Renderable>> update(const std::pair<uint32_t, uint32_t> &input, uint32_t clock) {
    std::vector<Renderable> out;
    return std::make_pair(std::move(*this), out);
  }

  Player player;
};

#endif
