#include "win_helpers.h"

#include <dbghelp.h>
#include <Windows.h>
#include <UserEnv.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#include "types.h"

using std::string;
using std::wstring;
using std::vector;

namespace sbat {
ScopedVirtualProtect::ScopedVirtualProtect(void* address, size_t size, uint32 new_protection)
    : address_(address),
      size_(size),
      old_protection_(0),
      has_errors_(false) {
  has_errors_ = VirtualProtect(address_, size_, new_protection,
      reinterpret_cast<PDWORD>(&old_protection_)) == 0;
}

ScopedVirtualProtect::~ScopedVirtualProtect() {
  if (!has_errors_) {
    VirtualProtect(address_, size_, old_protection_, reinterpret_cast<PDWORD>(&old_protection_));
  }
}

bool ScopedVirtualProtect::has_errors() const {
  return has_errors_;
}

ScopedVirtualAlloc::ScopedVirtualAlloc(HANDLE process_handle, void* address, size_t size,
  uint32 allocation_type, uint32 protection)
    : process_handle_(process_handle),
      alloc_(VirtualAllocEx(process_handle, address, size, allocation_type, protection)) {
}

ScopedVirtualAlloc::~ScopedVirtualAlloc() {
  if (alloc_ != nullptr) {
    VirtualFreeEx(process_handle_, alloc_, 0, MEM_FREE);
  }
}

WinHdc::WinHdc(HWND window)
  : window_(window),
    hdc_(GetDC(window)) {
}

WinHdc::~WinHdc() {
  ReleaseDC(window_, hdc_);
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

bool WindowsError::is_error() const {
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

  char* message_buffer;
  uint32 buffer_len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code_, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    reinterpret_cast<char*>(&message_buffer), 0, nullptr);
  if (message_buffer != nullptr) {
    size_t total_len = buffer_len + location_.size() + 3 + 1;
    char* out_buffer = new char[total_len];
    _snprintf_s(out_buffer, total_len, _TRUNCATE, "[%s] %s", location_.c_str(), message_buffer);
    string result(out_buffer);
    #pragma warning(suppress: 6280)
    LocalFree(message_buffer);
    delete[] out_buffer;

    return result;
  } else {
    // In some cases, the code we have is not actually a system code. For these, we will simply
    // print the error code to a string
    size_t total_len = 10 + location_.size() + 3 + 1;
    message_buffer = new char[total_len];
    _snprintf_s(message_buffer, total_len, _TRUNCATE, "[%s] 0x%08x", location_.c_str(), code_);
    string result(message_buffer);
    delete[] message_buffer;
    return message_buffer;
  }
}

struct InjectContext {
  wchar_t dll_path[MAX_PATH];
  char inject_proc_name[256];

  HMODULE (WINAPI* LoadLibraryW)(LPCWSTR lib_filename);
  FARPROC (WINAPI* GetProcAddress)(HMODULE module_handle, LPCSTR proc_name);
  DWORD (WINAPI* GetLastError)();
};

// Thread proc function for injecting, looks like:
// int InjectProc(InjectContext* context);
const byte inject_proc[] = {
  0x55,                                 // PUSH EBP
  0x8B, 0xEC,                           // MOV EBP, ESP
  0x83, 0xEC, 0x08,                     // SUB ESP, 8
  0x8B, 0x45, 0x08,                     // MOV EAX, &context->dll_path
  0x50,                                 // PUSH EAX
  0x8B, 0x4D, 0x08,                     // MOV ECX, context
  0x8B, 0x91, 0x08, 0x03, 0x00, 0x00,   // MOV EDX, context->LoadLibraryW
  0xFF, 0xD2,                           // CALL LoadLibraryW(&context->dll_path)
  0x89, 0x45, 0xFC,                     // MOV [LOCAL.1], EAX (module_handle)
  0x83, 0x7D, 0xFC, 0x00,               // CMP [LOCAL.1], 0
  0x75, 0x0D,                           // JNZ LOADLIB_SUCCESS
  0x8B, 0x45, 0x08,                     // MOV EAX, context
  0x8B, 0x88, 0x10, 0x03, 0x00, 0x00,   // MOV ECX, context->GetLasttError
  0xFF, 0xD1,                           // CALL GetLastError()
  0xEB, 0x34,                           // JMP EXIT
// LOADLIB_SUCCESS:
  0x8B, 0x55, 0x08,                     // MOV EDX, context
  0x81, 0xC2, 0x08, 0x02, 0x00, 0x00,   // ADD EDX, 208 (EDX = &context->inject_proc_name)
  0x52,                                 // PUSH EDX
  0x8B, 0x45, 0xFC,                     // MOV EAX, module_handle
  0x50,                                 // PUSH EAX
  0x8B, 0x4D, 0x08,                     // MOV ECX, context
  0x8B, 0x91, 0x0C, 0x03, 0x00, 0x00,   // MOV EDX, context->GetProcAddress
  0xFF, 0xD2,                           // CALL GetProcAddress(module_handle, inject_proc_name)
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

Process::Process(const wstring& app_path, const wstring& arguments, bool launch_suspended,
    const wstring& current_dir, const vector<wstring>& environment)
    : process_handle_(),
      thread_handle_(),
      error_() {
}

Process::~Process() {
}

bool Process::has_errors() const {
  return error_.is_error();
}

WindowsError Process::error() const {
  return error_;
}

WindowsError Process::InjectDll(const wstring& dll_path, const string& inject_function_name,
    const string& error_dump_path) {
  if (has_errors()) {
    return error();
  }

  InjectContext context;
  wchar_t* buf = lstrcpynW(context.dll_path, dll_path.c_str(), MAX_PATH);
  if (buf == nullptr) {
    return WindowsError("InjectDll -> lstrcpynW", ERROR_NOT_ENOUGH_MEMORY);
  }
  strcpy_s(context.inject_proc_name, inject_function_name.c_str());
  context.LoadLibraryW = LoadLibraryW;
  context.GetProcAddress = GetProcAddress;
  context.GetLastError = GetLastError;

  SIZE_T alloc_size = sizeof(context) + sizeof(inject_proc);
  ScopedVirtualAlloc remote_context(process_handle_.get(), nullptr, alloc_size, MEM_COMMIT,
      PAGE_EXECUTE_READWRITE);
  if (remote_context.has_errors()) {
    return WindowsError("InjectDll -> VirtualAllocEx", GetLastError());
  }

  SIZE_T bytes_written;
  BOOL success = WriteProcessMemory(process_handle_.get(), remote_context.get(), &context,
      sizeof(context), &bytes_written);
  if (!success || bytes_written != sizeof(context)) {
    return WindowsError("InjectDll -> WriteProcessMemory(InjectContext)", GetLastError());
  }

  void* remote_proc = reinterpret_cast<byte*>(remote_context.get()) + sizeof(context);
  success = WriteProcessMemory(process_handle_.get(), remote_proc, inject_proc,
      sizeof(inject_proc), &bytes_written);
  if (!success || bytes_written != sizeof(inject_proc)) {
    return WindowsError("InjectDll -> WriteProcessMemory(Proc)", GetLastError());
  }

  WinHandle thread_handle(CreateRemoteThread(process_handle_.get(), NULL, 0,
      reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_proc), remote_context.get(), 0,  nullptr));
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

WindowsError Process::Resume() {
  if (has_errors()) {
    return error();
  }

  if (ResumeThread(thread_handle_.get()) == -1) {
    return WindowsError("Process Resume -> ResumeThread", GetLastError());
  }

  return WindowsError();
}

WindowsError Process::Terminate() {
  if (has_errors()) {
    return error();
  }

  if (TerminateProcess(process_handle_.get(), 0) == 0) {
    return WindowsError("Process Terminate -> TerminateThread", GetLastError());
  }

  return WindowsError();
}

WindowsError Process::WaitForExit(uint32 max_wait_ms, bool* timed_out) {
  if (has_errors()) {
    return error();
  }

  if (timed_out != nullptr) {
    *timed_out = false;
  }

  DWORD result = WaitForSingleObject(process_handle_.get(), max_wait_ms);
  if (result == WAIT_TIMEOUT && timed_out != nullptr) {
    *timed_out = true;
    return WindowsError("WaitForExit -> WaitForSingleObject", WAIT_TIMEOUT);
  } else if (result == WAIT_FAILED) {
    return WindowsError("WaitForExit -> WaitForSingleObject", GetLastError());
  }

  return WindowsError();
}

WindowsError Process::GetExitCode(uint32* exit_code) {
  if (has_errors()) {
    return error();
  }

  BOOL result = GetExitCodeProcess(process_handle_.get(), reinterpret_cast<LPDWORD>(exit_code));
  if (result == FALSE) {
    return WindowsError("GetExitCode -> GetExitCodeProcess", GetLastError());
  }

  return WindowsError();
}
}  // namespace sbat