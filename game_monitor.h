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

protected:
  virtual void Execute();

private:
  // Disable copying
  GameMonitor(const GameMonitor&) = delete;
  GameMonitor& operator=(const GameMonitor&) = delete;

  void UpdateLocalTime();
  void DrawLocalTime();

  BroodWar bw_;
  // Access only on GameMonitor thread
  bool wasInGame_;
  // Acccess only on BW render thread
  std::array<char,128> cachedLocalTime_;
  uint64 localTimeValidUntil_;
};

}  // namespace apm