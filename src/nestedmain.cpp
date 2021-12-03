#include <memory>
#include <string>

#include <windows.h>

#include <strsafe.h>
#include <versionhelpers.h>

#include "basewindow.h"

void Log(const wchar_t *format, ...) {
  wchar_t linebuf[1024];
  va_list v;
  va_start(v, format);
  ::StringCbVPrintfW(linebuf, sizeof(linebuf), format, v);
  ::OutputDebugStringW(linebuf);
  va_end(v);
}

class MainWindow : public BaseWindow<MainWindow> {
  bool OnCreate() { return true; }

public:
  MainWindow() = default;

  LPCWSTR ClassName() const { return L"MainWindow"; }

  LRESULT HandleMessage(UINT msg, WPARAM w, LPARAM l) {
    LRESULT ret = 0;
    switch (msg) {
    case WM_CREATE:
      if (!OnCreate()) {
        return -1;
      }
      break;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      break;
    default:
      ret = ::DefWindowProcW(hwnd(), msg, w, l);
      break;
    }
    return ret;
  }
};

void ClientMain() {
  if (auto p = std::make_unique<MainWindow>()) {
    if (p->Create(L"MainWindow Title", WS_OVERLAPPEDWINDOW, /*style_ex*/ 0,
                  CW_USEDEFAULT, 0, 486, 300)) {

      ::ShowWindow(p->hwnd(), SW_SHOW);
      MSG msg;
      for (;;) {
        BOOL ret = ::GetMessageW(&msg, nullptr, 0, 0);
        if (!ret) {
          break; // Legit quit
        } else if (ret == -1) {
          Log(L"GetMessageW failed - %08x\n", GetLastError());
          break;
        }
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
      }
    }
  }
}

struct HandleCloser {
  typedef HANDLE pointer;
  void operator()(HANDLE h) {
    if (h) {
      ::CloseHandle(h);
    }
  }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR args, int) {
  const int level = (args && args[0]) ? _wtoi(args) : 0;
  if (level <= 1 || level > 5) {
    ClientMain();
    return 0;
  }

  wchar_t path[MAX_PATH];
  DWORD pathLen = ::GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (!pathLen) {
    Log(L"GetModuleFileNameW failed - %08lx\n", ::GetLastError());
    return 1;
  }

  std::unique_ptr<HANDLE, HandleCloser> job(::CreateJobObjectW(
      /*lpJobAttributes*/ nullptr,
      /*lpName*/ nullptr));

  DWORD jobFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
  jobInfo.BasicLimitInformation.LimitFlags = jobFlags;
  if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &jobInfo, sizeof(jobInfo))) {
    Log(L"SetInformationJobObject failed - %08lx\n", ::GetLastError());
    return 1;
  }

  std::unique_ptr<wchar_t[]> params(new wchar_t[pathLen + 10]);
  memcpy(params.get(), path, pathLen * sizeof(wchar_t));
  params[pathLen] = ' ';
  params[pathLen + 1] = static_cast<wchar_t>(L'0' + level - 1);
  params[pathLen + 2] = 0;

  DWORD creationFlags = CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB;

  PROCESS_INFORMATION pi = {};
  STARTUPINFOW si = {sizeof(si)};
  if (!::CreateProcessW(path, params.get(),
                        /*lpProcessAttributes*/ nullptr,
                        /*lpThreadAttributes*/ nullptr,
                        /*bInheritHandles*/ FALSE, creationFlags,
                        /*lpEnvironment*/ nullptr,
                        /*lpCurrentDirectory*/ nullptr, &si, &pi)) {
    Log(L"CreateProcess failed - %08lx\n", ::GetLastError());
    return 1;
  }

  if (!::AssignProcessToJobObject(job.get(), pi.hProcess)) {
    Log(L"AssignProcessToJobObject failed - %08lx\n", ::GetLastError());
    return 1;
  }

  ::ResumeThread(pi.hThread);
  ::CloseHandle(pi.hThread);

  ::WaitForSingleObject(pi.hProcess, INFINITE);
  ::CloseHandle(pi.hProcess);
  return 0;
}
