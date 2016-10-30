#pragma once

#include <Windows.h>
#include <array>

#include "./brood_war.h"
#include "./types.h"
#include "./win_helpers.h"

namespace apm {

class GameMonitor : public sbat::WindowsThread {
public:
  explicit GameMonitor(BroodWar bw);
  virtual ~GameMonitor();

  void Draw();
  void RefreshScreen();
  void OnAction(byte actionType);

protected:
  virtual void Execute();

private:
  // Disable copying
  GameMonitor(const GameMonitor&) = delete;
  GameMonitor& operator=(const GameMonitor&) = delete;

  void InitGameData();
  void UpdateLocalTime();
  void DrawLocalTime();
  void UpdateGameTime();
  void DrawGameTime();
  void CalculateApm();
  void DrawApm();

  uint32 GetDisplayStormId();
  
  bool IsObsMode();
  bool IsObserver(uint32 stormId);

  BroodWar bw_;
  // Access only on GameMonitor thread
  bool wasInGame_;

  // Acccess only on BW game loop thread
  std::array<char, 128> cachedLocalTime_;
  uint64 localTimeValidUntil_;
  std::array<char, 128> cachedGameTime_;
  uint32 gameTimeValidUntil_;

  std::array<double, 12> apmCounter_;
  std::array<std::string, 12> apmStrings_;
  uint32 apmCalcTime_;
};

}  // namespace apm