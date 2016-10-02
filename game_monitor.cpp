#include "game_monitor.h"

#include <Windows.h>

#include "./brood_war.h"
#include "./types.h"
#include "./win_helpers.h"

namespace apm {

GameMonitor::GameMonitor(BroodWar bw)
  : bw_(std::move(bw)),
    wasInGame_(false) {
}

GameMonitor::~GameMonitor() {
}

void GameMonitor::Execute() {
  while (!isTerminated()) {
    if (wasInGame_ && !bw_.isInGame) {
      wasInGame_ = false;
      bw_.RestoreHooks();
    } else if (!wasInGame_ && bw_.isInGame) {
      wasInGame_ = true;
      bw_.InjectHooks();
    }
    Sleep(200);
  }
}

const uint32 ANIM_TIME_MILLIS = 3000;

void GameMonitor::Draw() {
  BwFont backupFont = bw_.curFont;
  bw_.SetFont(bw_.fontUltraLarge);

  uint32 curTimeMillis = bw_.gameTimeTicks * 42;
  bool backwards = (curTimeMillis / ANIM_TIME_MILLIS) % 2 == 1;
  double animProgress =
      (curTimeMillis % ANIM_TIME_MILLIS) / (double) ANIM_TIME_MILLIS;
  uint32 xPos = (uint32) (animProgress * 510);
  if (backwards) {
    xPos = 510 - xPos;
  }
  bw_.DrawText(xPos, 20, "Hello world!");

  bw_.SetFont(backupFont);
}

void GameMonitor::RefreshScreen() {
  bw_.RefreshGameLayer();
}

} // namespace apm