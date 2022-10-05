#pragma once

#include <array>

namespace beetle_lights {
  using Renderable = std::pair<uint32_t, std::array<uint8_t, 3>>;
}
