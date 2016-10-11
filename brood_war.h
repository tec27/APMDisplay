#pragma once

#include <cstdint>
#include <functional>

#include "./func_hook.h"
#include "./types.h"

namespace apm {

template <typename T>
class DataOffset {
public:
  inline constexpr DataOffset() : offset_(0xDEADDEAD) {}
  inline constexpr explicit DataOffset(uintptr_t offset) : offset_(offset) {}
  ~DataOffset() {}

  inline void reset(uintptr_t offset) { offset_ = offset; }

  inline operator T() const { return *reinterpret_cast<T*>(offset_); }
  // This can be const because it does not affect the data in this object (only the data it points
  // to)
  inline T operator=(T value) const { return (*reinterpret_cast<T*>(offset_) = value); }
private:
  uintptr_t offset_;
};

using BwFont = uintptr_t;

struct BroodWar {
  BroodWar() = default;
  BroodWar(BroodWar&& bw) = default;

  // disallow copying
  BroodWar(const BroodWar&) = delete;
  BroodWar& operator=(const BroodWar&) = delete;

  void InjectHooks();
  void RestoreHooks();

  sbat::Detour drawDetour;
  sbat::Detour refreshScreenDetour;

  DataOffset<bool> isInGame;
  DataOffset<uint32> gameTimeTicks;
  DataOffset<uint32> lastTextWidth;

  DataOffset<BwFont> curFont;
  DataOffset<BwFont> fontUltraLarge;
  DataOffset<BwFont> fontLarge;
  DataOffset<BwFont> fontNormal;
  DataOffset<BwFont> fontMini;
  std::function<void(BwFont)> SetFont;

  #undef DrawText // BILL GATES WHY
  std::function<void(uint32 x, uint32 y, const std::string& text)> DrawText;
  std::function<bool()> RefreshGameLayer;
  std::function<uint32(const BroodWar& bw, const std::string& text)> GetTextWidth;
};

using DrawFn = void(__stdcall*)();
using RefreshFn = void(__stdcall*)();

inline BroodWar CreateV1161(DrawFn drawFunction, RefreshFn refreshFunction) {
  BroodWar bw;

  bw.drawDetour = std::move(sbat::Detour(sbat::Detour::Builder()
    .At(0x004BD614).To(drawFunction).RunningOriginalCodeBefore()));
  bw.refreshScreenDetour = std::move(sbat::Detour(sbat::Detour::Builder()
    .At(0x004D98DE).To(refreshFunction).RunningOriginalCodeBefore()));

  bw.isInGame.reset(0x006D11EC);
  bw.gameTimeTicks.reset(0x0057F23C);
  bw.lastTextWidth.reset(0x006CE108);

  bw.curFont.reset(0x006D5DDC);
  bw.fontUltraLarge.reset(0x006CE100);
  bw.fontLarge.reset(0x006CE0FC);
  bw.fontNormal.reset(0x006CE0F8);
  bw.fontMini.reset(0x006CE0F4);

  using SetFontFunc = void(__thiscall*)(BwFont font);
  bw.SetFont = [](BwFont font) {
    const auto BwSetFont = reinterpret_cast<SetFontFunc>(0x0041FB30);
    BwSetFont(font);
  };
  using DrawTextFunc = bool(__stdcall*)(uint32 y);
  bw.DrawText = [](uint32 x, uint32 y, const std::string& text) {
    const auto BwDrawText = reinterpret_cast<DrawTextFunc>(0x004202B0);
    const char* textPtr = text.c_str();
    __asm {
      pushad
      mov eax, [textPtr]
      mov esi, x
      push y
      mov ecx, [BwDrawText]
      call ecx
      popad
    }
  };
  using RefreshGameLayerFunc = bool(__cdecl*)();
  bw.RefreshGameLayer = reinterpret_cast<RefreshGameLayerFunc>(0x004BD350);
  using GetTextWidthFunc = bool(__cdecl*)();
  bw.GetTextWidth = [](const BroodWar& bw, const std::string& text) {
    const auto BwGetTextWidth = reinterpret_cast<GetTextWidthFunc>(0x0041F920);
    const char* textPtr = text.c_str();
    bw.lastTextWidth = 0;
    __asm {
      pushad
      mov eax, [textPtr]
      mov ecx, [BwGetTextWidth]
      call ecx
      popad
    }

    return bw.lastTextWidth;
  };

  return bw;
}

}  // namespace apm