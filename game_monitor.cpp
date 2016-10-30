#include "game_monitor.h"

#include <Windows.h>
#include <cmath>
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
    localTimeValidUntil_(0),
    cachedGameTime_(),
    gameTimeValidUntil_(0),
    apmCounter_(),
    apmStrings_(),
    apmCalcTime_(0) {
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
      InitGameData();
      bw_.InjectHooks();
    }
    Sleep(200);
  }
}

void GameMonitor::InitGameData() {
  localTimeValidUntil_ = 0;
  gameTimeValidUntil_ = 0;
  for (size_t i = 0; i < apmCounter_.size(); i++) {
    apmCounter_[i] = 0;
    apmStrings_[i] = "";
  }
  apmCalcTime_ = 0;
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

const uint32 LOCAL_CLOCK_X = 16;
const uint32 LOCAL_CLOCK_Y = 284;
void GameMonitor::DrawLocalTime() {
  bw_.SetFont(bw_.fontLarge);
  UpdateLocalTime();
  string localTimeStr = string("\x04") + cachedLocalTime_.data() + "\x01";
  bw_.DrawText(LOCAL_CLOCK_X, LOCAL_CLOCK_Y, localTimeStr);
}

void GameMonitor::UpdateGameTime() {
  const uint32 timeMillis = bw_.gameTimeTicks * 42;
  if (timeMillis <= gameTimeValidUntil_) {
    return;
  }

  uint32 seconds = timeMillis / 1000;
  uint32 minutes = seconds / 60;
  seconds -= (minutes * 60);

  int numChars = _snprintf_s(
    cachedGameTime_.data(), cachedGameTime_.size(), _TRUNCATE, "%.2d:%.2d", minutes, seconds);
  if (numChars == 0) {
    string errorStr = "ERROR";
    std::copy(errorStr.begin(), errorStr.end(), cachedGameTime_.begin());
  }

  gameTimeValidUntil_ = timeMillis + 1000 - (timeMillis % 1000);
}

const uint32 GAME_CLOCK_Y = 2;
void GameMonitor::DrawGameTime() {
  bw_.SetFont(bw_.fontLarge);
  UpdateGameTime();
  string gameTimeStr = string("\x04") + cachedGameTime_.data() + "\x01";
  uint32 xPos = (640 - bw_.GetTextWidth(bw_, "888:88")) / 2;
  bw_.DrawText(xPos, GAME_CLOCK_Y, gameTimeStr);
}

const double APM_INTERVAL = 0.95;  // time after which actions are worth 1/e (in minutes)
void GameMonitor::CalculateApm() {
  const uint32 timeMillis = bw_.gameTimeTicks * 42;
  if (apmCalcTime_ != 0 && timeMillis <= (apmCalcTime_ + 1000)) {
    return;
  }

  int32 timeDiff = static_cast<int32>(timeMillis - apmCalcTime_);
  for (size_t i = 0; i < apmCounter_.size(); i++) {
    apmCounter_[i] *= std::exp(-timeDiff / (APM_INTERVAL * 60000));
  }
  apmCalcTime_ = timeMillis;

  double gameDurationFactor =
      1 - std::exp(-static_cast<int32>(timeMillis) / (APM_INTERVAL * 60000));
  if (gameDurationFactor < 0.01) {
    gameDurationFactor = 0.01;
  }

  for (size_t i = 0; i < apmCounter_.size(); i++) {
    string playerName = bw_.GetPlayerName(i);
    bool updated = false;
    if (!playerName.empty()) {
      int32 apm = static_cast<int32>(apmCounter_[i] / (APM_INTERVAL * gameDurationFactor));
      if (i == bw_.myStormId && !IsObsMode()) {
        apmStrings_[i] = "\x04" "APM: " "\x07" + std::to_string(apm);
      } else {
        // TODO(tec27): colorize player names
        apmStrings_[i] = "\x04" + playerName + ": \x07" + std::to_string(apm);
      }
    } else {
      apmStrings_[i] = "";
    }
  }
}

const uint32 APM_X = 16;
const uint32 APM_Y = 4;
const uint32 LINE_SIZE = 12;
void GameMonitor::DrawApm() {
  bw_.SetFont(bw_.fontNormal);
  CalculateApm();
  if (IsObsMode()) {
    uint32 lineNum = 0;
    for (size_t i = 0; i < apmStrings_.size(); i++) {
      if (!apmStrings_[i].empty() && !IsObserver(i)) {
        bw_.DrawText(APM_X, APM_Y + lineNum * LINE_SIZE, apmStrings_[i]);
        lineNum++;
      }
    }
  } else if (GetDisplayStormId() < apmStrings_.size()) {
    bw_.DrawText(APM_X, APM_Y, apmStrings_[GetDisplayStormId()]);
  }
}

void GameMonitor::Draw() {
  BwFont backupFont = bw_.curFont;

  DrawLocalTime();
  DrawGameTime();
  DrawApm();

  bw_.SetFont(backupFont);
}

void GameMonitor::RefreshScreen() {
  bw_.RefreshGameLayer();
}

const byte ACTION_TYPE_KEEPALIVE = 0x37;
void GameMonitor::OnAction(byte actionType) {
  if (bw_.activeStormId >= apmCounter_.size() || actionType == ACTION_TYPE_KEEPALIVE) {
    return;
  }

  apmCounter_[bw_.activeStormId] += 1;
}

uint32 GameMonitor::GetDisplayStormId() {
  if (bw_.isInReplay) {
    return bw_.selectedStormId > 11 ? 12 : bw_.selectedStormId;
  } else {
    return bw_.myStormId;
  }
}

bool GameMonitor::IsObsMode() {
  if (bw_.activeStormId == 0xFFFFFFFF) {
    return bw_.isInReplay;
  } else {
    if (bw_.isInReplay) {
      return true;
    }

    uint32 stormId = bw_.myStormId;
    return IsObserver(stormId);
  }
}

bool GameMonitor::IsObserver(uint32 stormId) {
  // Handles both initial obs (UMS map), and "almost dead" obs, where people played the game but are
  // now without units and allied to people
  uint32 buildingsControlled = bw_.GetBuildingsControlled(stormId);
  uint32 population = bw_.GetPopulation(stormId);
  int32 minerals = bw_.GetMinerals(stormId);
  int32 vespene = bw_.GetVespene(stormId);
  // initial obs/ums
  return (buildingsControlled <= 1 && population <= 2 && minerals <= 50 && vespene == 0) ||
    // "almost dead" obs
    (buildingsControlled <= 1 && population == 0);
}

} // namespace apm