#pragma once

#include <Windows.h>

#include "./brood_war.h"
#include "./types.h"
#include "./win_helpers.h"

namespace apm {

class GameMonitor : public sbat::WindowsThread {
public:
  explicit GameMonitor(BroodWar bw);
  virtual ~GameMonitor();

protected:
  virtual void Execute();

private:
  // Disable copying
  GameMonitor(const GameMonitor&) = delete;
  GameMonitor& operator=(const GameMonitor&) = delete;

  BroodWar bw_;
  // Access only on GameMonitor thread
  bool wasInGame_;
};

}  // namespace apm