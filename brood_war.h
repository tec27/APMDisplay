#pragma once

#include <cstdint>

#include "types.h"

namespace apm {

template <typename T>
class DataOffset {
public:
  inline constexpr DataOffset() : offset_(0xDEADDEAD) {}
  inline constexpr DataOffset(uintptr_t offset) : offset_(offset) {}
  ~DataOffset() {}

  inline void reset(uintptr_t offset) { offset_ = offset; }

  inline operator T() const { return *reinterpret_cast<T*>(offset_); }
  // This can be const because it does not affect the data in this object (only the data it points
  // to)
  inline T operator=(T value) const { return (*reinterpret_cast<T*>(offset_) = value); }
private:
  uintptr_t offset_;
};

struct BroodWar {
  DataOffset<bool> isInGame;
};

inline BroodWar CreateV1161() {
  BroodWar bw;

  bw.isInGame.reset(0x006D11EC);

  return bw;
}

}  // namespace apm