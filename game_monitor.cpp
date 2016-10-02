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

void GameMonitor::Draw() {
  BwFont backupFont = bw_.curFont;
  bw_.SetFont(bw_.fontUltraLarge);
  bw_.DrawText(20, 20, "Hello world!");

  bw_.SetFont(backupFont);
}

} // namespace apm