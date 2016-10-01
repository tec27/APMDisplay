#include <Windows.h>
#include <memory>
#include <string>

#include "./types.h"
#include "./win_helpers.h"
#include "./func_hook.h"

using sbat::ScopedVirtualProtect;
using std::string;
using std::unique_ptr;

// We don't really work with all versions, but BWL doesn't allow for ranges, nor does Chaos support
// anything higher than 1.16.1
static const int32 kStarcraftBuild = -1; 
static const int32 kPluginMajor = 1;
static const int32 kPluginMinor = 0;
static const char* kPluginName = "APMDisplay (1.16.1)";
static const char* kDescription =
  "APMDisplay v1.0\r\n\r\n"
  "Displays a live APM overlay ingame, as well as a local and game time clocks.\r\n\r\n"
  "by tec27";
static const char* kUpdateUrl = "http://tec27.com/apmdisplay/";

#define BWL_FUNCTION extern "C" __declspec(dllexport)

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

BWL_FUNCTION void GetData(char* name, char* description, char* update_url) {
  strcpy_s(name, 1024, kPluginName);
  strcpy_s(description, 8192, kDescription);
  strcpy_s(update_url, 1024, kUpdateUrl);
}

BWL_FUNCTION bool ApplyPatchSuspended(HANDLE process_handle, uint32 process_id) {
  return true;
}

BWL_FUNCTION bool ApplyPatch(HANDLE process_handle, uint32 process_id) {
  return true;
}

BOOL WINAPI DllMain(HMODULE module_handle, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_DETACH) {
    // FreeHooks();
  }

  return true;
}
