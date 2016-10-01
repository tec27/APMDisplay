#include "./win_helpers.h"

#include <Windows.h>
#include <assert.h>
#include <dbghelp.h>
#include <process.h>
#include <UserEnv.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#include "./types.h"

using std::string;
using std::wstring;
using std::vector;

namespace sbat {
ScopedVirtualProtect::ScopedVirtualProtect(void* address, size_t size, uint32 newProtection)
    : address_(address),
      size_(size),
      oldProtection_(0),
      hasErrors_(false) {
  hasErrors_ = VirtualProtect(address_, size_, newProtection,
      reinterpret_cast<PDWORD>(&oldProtection_)) == 0;
}

ScopedVirtualProtect::~ScopedVirtualProtect() {
  if (!hasErrors_) {
    VirtualProtect(address_, size_, oldProtection_, reinterpret_cast<PDWORD>(&oldProtection_));
  }
}

bool ScopedVirtualProtect::hasErrors() const {
  return hasErrors_;
}

ScopedVirtualAlloc::ScopedVirtualAlloc(HANDLE processHandle, void* address, size_t size,
  uint32 allocationType, uint32 protection)
    : processHandle_(processHandle),
      alloc_(VirtualAllocEx(processHandle, address, size, allocationType, protection)) {
}

ScopedVirtualAlloc::~ScopedVirtualAlloc() {
  if (alloc_ != nullptr) {
    VirtualFreeEx(processHandle_, alloc_, 0, MEM_FREE);
  }
}

WinHandle::WinHandle(HANDLE handle)
   : handle_(handle) {
}

WinHandle::~WinHandle() {
  if (handle_ != NULL && handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
  }
}

void WinHandle::Reset(HANDLE handle) {
  if (handle_ != NULL && handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
  }
  handle_ = handle;
}

WindowsError::WindowsError(string location, uint32 error_code)
    : code_(error_code),
      location_(std::move(location)) {
}

bool WindowsError::isError() const {
  return code_ != 0;
}

uint32 WindowsError::code() const {
  return code_;
}

string WindowsError::location() const {
  return location_;
}

string WindowsError::message() const {
  if (code_ == 0) {
    return "No error";
  }

  char* messageBuffer;
  uint32 bufferLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code_, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    reinterpret_cast<char*>(&messageBuffer), 0, nullptr);
  if (messageBuffer != nullptr) {
    size_t totalLen = bufferLen + location_.size() + 3 + 1;
    char* outBuffer = new char[totalLen];
    _snprintf_s(outBuffer, totalLen, _TRUNCATE, "[%s] %s", location_.c_str(), messageBuffer);
    string result(outBuffer);
    #pragma warning(suppress: 6280)
    LocalFree(messageBuffer);
    delete[] outBuffer;

    return result;
  } else {
    // In some cases, the code we have is not actually a system code. For these, we will simply
    // print the error code to a string
    size_t totalLen = 10 + location_.size() + 3 + 1;
    messageBuffer = new char[totalLen];
    _snprintf_s(messageBuffer, totalLen, _TRUNCATE, "[%s] 0x%08x", location_.c_str(), code_);
    string result(messageBuffer);
    delete[] messageBuffer;
    return messageBuffer;
  }
}

WindowsThread::WindowsThread()
  : handle_(INVALID_HANDLE_VALUE),
    threadId_(0),
    started_(false),
    terminated_(true) {
}

WindowsThread::~WindowsThread() {
}

void WindowsThread::Start() {
  if (started_ || handle_ != INVALID_HANDLE_VALUE) {
    return;
  }

  uint32 threadId;
  handle_ = reinterpret_cast<HANDLE>(
    _beginthreadex(NULL, 0, WindowsThread::ThreadProc, this, 0, &threadId));
  threadId_ = threadId;
}

void WindowsThread::Terminate() {
  terminated_ = true;
}

void WindowsThread::Run() {
  assert(!started_);
  assert(terminated_);
  Setup();
  started_ = true;
  terminated_ = false;

  Execute();

  started_ = false;
  terminated_ = true;
  handle_ = INVALID_HANDLE_VALUE;
  threadId_ = 0;
}

unsigned __stdcall WindowsThread::ThreadProc(void* arg) {
  WindowsThread* thread = reinterpret_cast<WindowsThread*>(arg);
  thread->Run();
  return 1;
}

struct InjectContext {
  wchar_t dllPath[MAX_PATH];
  char injectProcName[256];

  HMODULE (WINAPI* LoadLibraryW)(LPCWSTR libFilename);
  FARPROC (WINAPI* GetProcAddress)(HMODULE moduleHandle, LPCSTR procName);
  DWORD (WINAPI* GetLastError)();
};

// Thread proc function for injecting, looks like:
// int InjectProc(InjectContext* context);
const byte injectProc[] = {
  0x55,                                 // PUSH EBP
  0x8B, 0xEC,                           // MOV EBP, ESP
  0x83, 0xEC, 0x08,                     // SUB ESP, 8
  0x8B, 0x45, 0x08,                     // MOV EAX, &context->dllPath
  0x50,                                 // PUSH EAX
  0x8B, 0x4D, 0x08,                     // MOV ECX, context
  0x8B, 0x91, 0x08, 0x03, 0x00, 0x00,   // MOV EDX, context->LoadLibraryW
  0xFF, 0xD2,                           // CALL LoadLibraryW(&context->dllPath)
  0x89, 0x45, 0xFC,                     // MOV [LOCAL.1], EAX (moduleHandle)
  0x83, 0x7D, 0xFC, 0x00,               // CMP [LOCAL.1], 0
  0x75, 0x0D,                           // JNZ LOADLIB_SUCCESS
  0x8B, 0x45, 0x08,                     // MOV EAX, context
  0x8B, 0x88, 0x10, 0x03, 0x00, 0x00,   // MOV ECX, context->GetLasttError
  0xFF, 0xD1,                           // CALL GetLastError()
  0xEB, 0x34,                           // JMP EXIT
// LOADLIB_SUCCESS:
  0x8B, 0x55, 0x08,                     // MOV EDX, context
  0x81, 0xC2, 0x08, 0x02, 0x00, 0x00,   // ADD EDX, 208 (EDX = &context->injectProcName)
  0x52,                                 // PUSH EDX
  0x8B, 0x45, 0xFC,                     // MOV EAX, moduleHandle
  0x50,                                 // PUSH EAX
  0x8B, 0x4D, 0x08,                     // MOV ECX, context
  0x8B, 0x91, 0x0C, 0x03, 0x00, 0x00,   // MOV EDX, context->GetProcAddress
  0xFF, 0xD2,                           // CALL GetProcAddress(moduleHandle, injectProcName)
  0x89, 0x45, 0xF8,                     // MOV [LOCAL.2], EAX (func)
  0x83, 0x7D, 0xF8, 0x00,               // CMP [LOCAL.2], 0
  0x75, 0x0D,                           // JNZ GETPROC_SUCCESS
  0x8B, 0x45, 0x08,                     // MOV EAX, context
  0x8B, 0x88, 0x10, 0x03, 0x00, 0x00,   // MOV ECX, context->GetLastError
  0xFF, 0xD1,                           // CALL GetLastError()
  0xEB, 0x05,                           // JMP EXIT
// GETPROC_SUCCESS:
  0xFF, 0x55, 0xF8,                     // CALL func
  0x33, 0xC0,                           // XOR EAX, EAX (return value = 0)
// EXIT:
  0x8B, 0xE5,                           // MOV ESP, EBP
  0x5D,                                 // POP EBP
  0xC2, 0x04, 0x00                      // RETN 4
};


WindowsError InjectDll(HANDLE processHandle, const wstring& dllPath,
  const string& injectFunctionName) {
  InjectContext context;
  wchar_t* buf = lstrcpynW(context.dllPath, dllPath.c_str(), MAX_PATH);
  if (buf == nullptr) {
    return WindowsError("InjectDll -> lstrcpynW", ERROR_NOT_ENOUGH_MEMORY);
  }
  strcpy_s(context.injectProcName, injectFunctionName.c_str());
  context.LoadLibraryW = LoadLibraryW;
  context.GetProcAddress = GetProcAddress;
  context.GetLastError = GetLastError;

  SIZE_T allocSize = sizeof(context) + sizeof(injectProc);
  ScopedVirtualAlloc remoteContext(processHandle, nullptr, allocSize, MEM_COMMIT,
      PAGE_EXECUTE_READWRITE);
  if (remoteContext.hasErrors()) {
    return WindowsError("InjectDll -> VirtualAllocEx", GetLastError());
  }

  SIZE_T bytesWritten;
  BOOL success = WriteProcessMemory(processHandle, remoteContext.get(), &context,
      sizeof(context), &bytesWritten);
  if (!success || bytesWritten != sizeof(context)) {
    return WindowsError("InjectDll -> WriteProcessMemory(InjectContext)", GetLastError());
  }

  void* remoteProc = reinterpret_cast<byte*>(remoteContext.get()) + sizeof(context);
  success = WriteProcessMemory(processHandle, remoteProc, injectProc,
      sizeof(injectProc), &bytesWritten);
  if (!success || bytesWritten != sizeof(injectProc)) {
    return WindowsError("InjectDll -> WriteProcessMemory(Proc)", GetLastError());
  }

  WinHandle thread_handle(CreateRemoteThread(processHandle, NULL, 0,
      reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteProc), remoteContext.get(), 0,  nullptr));
  if (thread_handle.get() == nullptr) {
    return WindowsError("InjectDll -> CreateRemoteThread", GetLastError());
  }

  uint32 wait_result = WaitForSingleObject(thread_handle.get(), 15000);
  if (wait_result == WAIT_TIMEOUT) {
    return WindowsError("InjectDll -> WaitForSingleObject", WAIT_TIMEOUT);
  } else if (wait_result == WAIT_FAILED) {
    return WindowsError("InjectDll -> WaitForSingleObject", GetLastError());
  }

  DWORD exit_code;
  uint32 exit_result = GetExitCodeThread(thread_handle.get(), &exit_code);
  if (exit_result == 0) {
    return WindowsError("InjectDll -> GetExitCodeThread", GetLastError());
  }

  return WindowsError("InjectDll -> injection proc exit code", exit_code);
}

}  // namespace sbat