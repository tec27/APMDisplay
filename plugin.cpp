#include <Windows.h>
#include <memory>
#include <string>
#include <vector>

#include "./brood_war.h"
#include "./game_monitor.h"
#include "./func_hook.h"
#include "./types.h"
#include "./win_helpers.h"


using apm::GameMonitor;
using sbat::InjectDll;
using sbat::ScopedVirtualProtect;
using std::string;
using std::unique_ptr;
using std::vector;

// We don't really work with all versions, but BWL doesn't allow for ranges, nor does Chaos support
// anything higher than 1.16.1
static const int32 kStarcraftBuild = -1; 
static const int32 kPluginMajor = 1;
static const int32 kPluginMinor = 0;
static const char* kPluginName = "APMDisplay";
static const char* kDescription =
  "APMDisplay v1.0\r\n\r\n"
  "Displays a live APM overlay ingame, as well as local and game time clocks.\r\n\r\n"
  "by tec27";
static const char* kUpdateUrl = "http://tec27.com/apmdisplay/";

#define BWL_FUNCTION extern "C" __declspec(dllexport)

BWL_FUNCTION void OnInject();

struct BwLauncherData {
  uint32 pluginApi;
  int32 starcraftBuild;
  bool runsOutsideBwlauncher;
  bool hasConfigDialog;
};

BWL_FUNCTION void GetPluginAPI(BwLauncherData* data) {
  data->pluginApi = 4;
  data->starcraftBuild = kStarcraftBuild;
  data->hasConfigDialog = false;
  data->runsOutsideBwlauncher = false;
}

BWL_FUNCTION void GetData(char* name, char* description, char* updateUrl) {
  strcpy_s(name, 1024, kPluginName);
  strcpy_s(description, 8192, kDescription);
  strcpy_s(updateUrl, 1024, kUpdateUrl);
}

BWL_FUNCTION bool ApplyPatchSuspended(HANDLE processHandle, uint32 processId) {
  HMODULE selfHandle;
  wchar_t selfPath[MAX_PATH];
  BOOL gotHandle = GetModuleHandleExW(
    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    reinterpret_cast<LPCWSTR>(&OnInject), &selfHandle);
  if (!gotHandle) {
    return false;
  }
  DWORD copied = GetModuleFileNameW(selfHandle, selfPath, sizeof(selfPath) / sizeof(wchar_t));
  if (copied == 0) {
    return false;
  }

  auto result = InjectDll(processHandle, selfPath, "OnInject");
  if (result.isError()) {
    return false;
  }

  return true;
}

BWL_FUNCTION bool ApplyPatch(HANDLE processHandle, uint32 processId) {
  
  return true;
}

unique_ptr<GameMonitor> gameMonitor = unique_ptr<GameMonitor>();

bool VersionsEqual(
    VS_FIXEDFILEINFO* fileInfo, uint16 majorHi, uint16 majorLo, uint16 minorHi, uint16 minorLo) {
  return (HIWORD(fileInfo->dwProductVersionMS) == majorHi &&
      LOWORD(fileInfo->dwProductVersionMS) == majorLo &&
      HIWORD(fileInfo->dwProductVersionLS) == minorHi &&
      LOWORD(fileInfo->dwProductVersionLS) == minorLo);
}

BWL_FUNCTION void OnInject() {
  wchar_t bwPath[MAX_PATH];
  HMODULE bwHandle = GetModuleHandle(NULL);
  if (!bwHandle) {
    return;
  }
  DWORD copied = GetModuleFileNameW(bwHandle, bwPath, sizeof(bwPath) / sizeof(wchar_t));
  if (copied == 0) {
    return;
  }

  int32 infoSize = GetFileVersionInfoSizeW(bwPath, nullptr);
  if (infoSize == 0) {
    return;
  }
  vector<byte> infoData(infoSize);
  int result = GetFileVersionInfoW(bwPath, 0, infoData.size(), &infoData[0]);
  if (result == 0) {
    return;
  }

  uint32 length;
  VS_FIXEDFILEINFO* fileInfo;
  result = VerQueryValueW(&infoData[0], L"\\", reinterpret_cast<void**>(&fileInfo), &length);
  if (result == 0 || length == 0) {
    return;
  }

  apm::DrawFn drawFn = []() {
    gameMonitor->Draw();
  };
  apm::RefreshFn refreshFn = []() {
    gameMonitor->RefreshScreen();
  };
  apm::OnActionFn onActionFn = [](const byte* actionType) {
    gameMonitor->OnAction(*actionType);
  };

  if (VersionsEqual(fileInfo, 1, 16, 1, 1)) {
    gameMonitor.reset(new GameMonitor(apm::CreateV1161(drawFn, refreshFn, onActionFn)));
  } else {
    return;
  }
  
  gameMonitor->Start();
}