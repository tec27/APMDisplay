#pragma once

#include <Windows.h>
#include <Wtsapi32.h>
#include <string>
#include <vector>
#include "types.h"

// Helper Types/Functions for Windows things, to make them easier/safer to use

namespace sbat {
// Type for simpler VirtualProtect -> restore old protection usage.
class ScopedVirtualProtect {
public:
  ScopedVirtualProtect(void* address, size_t size, uint32 newProtection);
  ~ScopedVirtualProtect();
  bool hasErrors() const;
private:
  // Disable copying
  ScopedVirtualProtect(const ScopedVirtualProtect&) = delete;
  ScopedVirtualProtect& operator=(const ScopedVirtualProtect&) = delete;

  void* address_;
  size_t size_;
  uint32 oldProtection_;
  bool hasErrors_;
};

class ScopedVirtualAlloc {
public:
  ScopedVirtualAlloc(HANDLE processHandle, void* address, size_t size, uint32 allocationType,
     uint32 protection);
  ~ScopedVirtualAlloc();

  bool hasErrors() const { return alloc_ == nullptr; }
  void* get() const { return alloc_; }
private:
  // Disable copying
  ScopedVirtualAlloc(const ScopedVirtualAlloc&) = delete;
  ScopedVirtualAlloc& operator=(const ScopedVirtualAlloc&) = delete;

  HANDLE processHandle_;
  void* alloc_;
};

class WinHandle {
public:
  WinHandle() : WinHandle(INVALID_HANDLE_VALUE) {}
  explicit WinHandle(HANDLE handle);
  ~WinHandle();

  HANDLE get() const { return handle_; }
  // Sets a new handle for this container, closing the previous one if one was set
  void Reset(HANDLE handle);
private:
  // Disable copying
  WinHandle(const WinHandle&) = delete;
  WinHandle& operator=(const WinHandle&) = delete;

  HANDLE handle_;
};

class WindowsError {
public:
  WindowsError(std::string location, uint32 errorCode);
  WindowsError() : WindowsError("", 0) {}

  bool isError() const;
  uint32 code() const;
  std::string message() const;
  std::string location() const;

private:
  uint32 code_;
  std::string location_;
};


WindowsError InjectDll(HANDLE processHandle, const std::wstring& dllPath,
  const std::string& injectFunctionName, const std::string& errorDumpPath);


}  // namespace sbat
