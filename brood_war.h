#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <string>

#include "./func_hook.h"
#include "./types.h"

namespace apm {
#pragma pack(1)
struct PlayerInfo {
  uint32 playerId;
  uint32 stormId;
  uint8 type;
  uint8 race;
  uint8 team;
  char name[25];
};
#pragma pack()

template <typename T>
class DataOffset {
public:
  inline constexpr DataOffset() : offset_(0xDEADDEAD) {}
  inline constexpr explicit DataOffset(uintptr_t offset) : offset_(offset) {}
  ~DataOffset() {}

  inline void reset(uintptr_t offset) { offset_ = offset; }

  inline T* get() const { return reinterpret_cast<T*>(offset_); }
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

  inline std::string GetPlayerName(int index) {
    assert(index >= 0 && index < 12);
    PlayerInfo* player = &firstPlayerInfo.get()[index];
    return std::move(std::string(player->name));
  }
  inline uint32 GetPlayerStormId(int index) {
    assert(index >= 0 && index < 12);
    PlayerInfo* player = &firstPlayerInfo.get()[index];
    return player->stormId;
  }
  inline uint32 GetBuildingsControlled(int index) {
    assert(index >= 0 && index < 12);
    return buildingsControlled.get()[index];
  }
  inline uint32 GetPopulation(int index) {
    assert(index >= 0 && index < 12);
    return population.get()[index];
  }
  inline int32 GetMinerals(int index) {
    assert(index >= 0 && index < 12);
    return minerals.get()[index];
  }
  inline int32 GetVespene(int index) {
    assert(index >= 0 && index < 12);
    return vespene.get()[index];
  }
  inline uint8 GetPlayerColor(int index) {
    assert(index >= 0 && index < 12);
    return firstPlayerColor.get()[index];
  }

  sbat::Detour drawDetour;
  sbat::Detour refreshScreenDetour;
  sbat::Detour onActionDetour;

  DataOffset<bool> isInGame;
  DataOffset<bool> isInReplay;
  DataOffset<uint32> gameTimeTicks;
  DataOffset<uint32> lastTextWidth;
  DataOffset<uint32> activeStormId;
  DataOffset<uint32> myPlayerId;
  DataOffset<PlayerInfo> firstPlayerInfo;
  DataOffset<uint32> buildingsControlled;
  DataOffset<uint32> population;
  DataOffset<uint32> minerals;
  DataOffset<uint32> vespene;
  DataOffset<uint8> firstPlayerColor;

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
using OnActionFn = void(__stdcall*)(const byte* actionType);

inline BroodWar CreateV1161(
  DrawFn drawFunction, RefreshFn refreshFunction, OnActionFn onActionFunction) {
  BroodWar bw;

  bw.drawDetour = std::move(sbat::Detour(sbat::Detour::Builder()
    .At(0x004BD614).To(drawFunction).RunningOriginalCodeBefore()));
  bw.refreshScreenDetour = std::move(sbat::Detour(sbat::Detour::Builder()
    .At(0x004D98DE).To(refreshFunction).RunningOriginalCodeBefore()));
  bw.onActionDetour = std::move(sbat::Detour(sbat::Detour::Builder()
    .At(0x00486D8B)
    .To(onActionFunction)
    .WithArgument(sbat::RegisterArgument::Ebx) // action type
    .RunningOriginalCodeAfter()));

  bw.isInGame.reset(0x006D11EC);
  bw.isInReplay.reset(0x006D0F14);
  bw.gameTimeTicks.reset(0x0057F23C);
  bw.lastTextWidth.reset(0x006CE108);
  bw.activeStormId.reset(0x0051267C);
  bw.myPlayerId.reset(0x00512688);
  bw.firstPlayerInfo.reset(0x0057EEE0);
  bw.buildingsControlled.reset(0x00581F34);
  bw.population.reset(0x00581E14);
  bw.minerals.reset(0x0057F0F0);
  bw.vespene.reset(0x0057F120);
  bw.firstPlayerColor.reset(0x00581DD6);

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