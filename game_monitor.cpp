#include "game_monitor.h"

#include <Windows.h>

#include "./brood_war.h"
#include "./types.h"
#include "./win_helpers.h"

namespace apm {

GameMonitor::GameMonitor(BroodWar bw)
  : bw_(bw),
    wasInGame_(false) {
}

GameMonitor::~GameMonitor() {
}

void GameMonitor::Execute() {
  while (!isTerminated()) {
    if (wasInGame_ && !bw_.isInGame) {
      wasInGame_ = false;
      MessageBoxA(NULL, "omg why'd you leave", "hi", MB_OK);
    } else if (!wasInGame_ && bw_.isInGame) {
      wasInGame_ = true;
      MessageBoxA(NULL, "way to get in that game", "hi", MB_OK);
    }
    Sleep(100);
  }
}

} // namespace apm