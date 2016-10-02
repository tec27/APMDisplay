#pragma once

#include <cstdint>

#include "./func_hook.h"
#include "./types.h"

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
  BroodWar() = default;
  BroodWar(BroodWar&& bw) = default;

  // disallow copying
  BroodWar(const BroodWar&) = delete;
  BroodWar& operator=(const BroodWar&) = delete;

  void InjectHooks();
  void RestoreHooks();

  sbat::Detour drawDetour;
  DataOffset<bool> isInGame;
};

using DrawFn = void(__stdcall*)();

inline BroodWar CreateV1161(DrawFn drawFunction) {
  BroodWar bw;

  bw.drawDetour = std::move(sbat::Detour(sbat::Detour::Builder()
    .At(0x004BD614).To(drawFunction).RunningOriginalCodeBefore()));

  bw.isInGame.reset(0x006D11EC);

  return bw;
}

}  // namespace apm