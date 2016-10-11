#include "game_monitor.h"

#include <Windows.h>
#include <string>

#include "./brood_war.h"
#include "./types.h"
#include "./win_helpers.h"

namespace apm {

using std::string;

GameMonitor::GameMonitor(BroodWar bw)
  : bw_(std::move(bw)),
    wasInGame_(false),
    cachedLocalTime_(),
    localTimeValidUntil_(0) {
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

void GameMonitor::UpdateLocalTime() {
  uint64 tickCount = GetTickCount64();
  if (tickCount <= localTimeValidUntil_) {
    return;
  }

  SYSTEMTIME localTime = SYSTEMTIME();
  GetLocalTime(&localTime);
  WCHAR wideStr[64];
  bool success = false;
  int numChars =
    GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &localTime, nullptr, wideStr, 64);
  if (numChars != 0) {
    int numBytes = WideCharToMultiByte(
      CP_THREAD_ACP, NULL, wideStr, -1, &cachedLocalTime_[0], cachedLocalTime_.size(), NULL, NULL);
    if (numBytes != 0) {
      success = true;
    }    
  }

  if (!success) {
    string errorStr = "ERROR";
    std::copy(errorStr.begin(), errorStr.end(), cachedLocalTime_.begin());
  }

  tickCount = GetTickCount64();
  tickCount += (60 - localTime.wSecond) * 1000;
  localTimeValidUntil_ = tickCount;
}

const uint32 LOCAL_CLOCK_X = 14;
const uint32 LOCAL_CLOCK_Y = 284;

void GameMonitor::DrawLocalTime() {
  bw_.SetFont(bw_.fontLarge);
  UpdateLocalTime();
  string localTimeStr = string("\x04") + cachedLocalTime_.data() + "\x01";
  bw_.DrawText(LOCAL_CLOCK_X, LOCAL_CLOCK_Y, localTimeStr);
}

void GameMonitor::Draw() {
  BwFont backupFont = bw_.curFont;

  DrawLocalTime();

  bw_.SetFont(backupFont);
}

void GameMonitor::RefreshScreen() {
  bw_.RefreshGameLayer();
}

} // namespace apm