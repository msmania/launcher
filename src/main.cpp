#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

#include "args.h"
#include "blob.h"
#include <atlbase.h>
#include <comutil.h>
#include <exdisp.h>
#include <shldisp.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
#include <strsafe.h>

void Log(LPCWSTR format, ...) {
  va_list v;
  va_start(v, format);
  vwprintf(format, v);
  va_end(v);
}

void LogDebug(LPCWSTR format, ...) {
  WCHAR linebuf[1024];
  va_list v;
  va_start(v, format);
  wvsprintf(linebuf, format, v);
  va_end(v);
  OutputDebugString(linebuf);
}

void LaunchViaShell(const _bstr_t &aPath, const _variant_t &aArgs,
                    const _variant_t &aVerb, const _variant_t &aWorkingDir,
                    const _variant_t &aShowCmd) {
  // NB: Explorer may be a local server, not an inproc server
  CComPtr<IShellWindows> shellWindows;
  HRESULT hr = shellWindows.CoCreateInstance(
      CLSID_ShellWindows, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER);
  if (FAILED(hr)) {
    Log(L"CoCreateInstance(CLSID_ShellWindows) failed - %08x\n", hr);
    return;
  }

  // 1. Find the shell view for the desktop.
  _variant_t loc(int(CSIDL_DESKTOP));
  _variant_t empty;
  long hwnd;
  CComPtr<IDispatch> dispDesktop;
  hr = shellWindows->FindWindowSW(&loc, &empty, SWC_DESKTOP, &hwnd,
                                  SWFO_NEEDDISPATCH, &dispDesktop);
  if (FAILED(hr)) {
    Log(L"IShellWindows::FindWindowSW failed - %08x\n", hr);
    return;
  }

  if (hr == S_FALSE) {
    Log(L"The call succeeded but the window was not found.\n");
    return;
  }

  CComPtr<IServiceProvider> servProv;
  hr = dispDesktop->QueryInterface(IID_IServiceProvider,
                                   reinterpret_cast<void **>(&servProv));
  if (!servProv) {
    Log(L"QueryInterface(IServiceProvider) failed - %08x\n", hr);
    return;
  }

  CComPtr<IShellBrowser> browser;
  hr = servProv->QueryService(SID_STopLevelBrowser, IID_IShellBrowser,
                              reinterpret_cast<void **>(&browser));
  if (FAILED(hr)) {
    Log(L"IServiceProvider::QueryService failed - %08x\n", hr);
    return;
  }

  CComPtr<IShellView> activeShellView;
  hr = browser->QueryActiveShellView(&activeShellView);
  if (FAILED(hr)) {
    Log(L"IShellBrowser::QueryActiveShellView failed - %08x\n", hr);
    return;
  }

  // 2. Get the automation object for the desktop.
  CComPtr<IDispatch> dispView;
  hr = activeShellView->GetItemObject(SVGIO_BACKGROUND, IID_IDispatch,
                                      reinterpret_cast<void **>(&dispView));
  if (FAILED(hr)) {
    Log(L"IShellView::GetItemObject failed - %08x\n", hr);
    return;
  }

  CComPtr<IShellFolderViewDual> folderView;
  hr = dispView->QueryInterface(IID_IShellFolderViewDual,
                                reinterpret_cast<void **>(&folderView));
  if (FAILED(hr)) {
    Log(L"IDispatch::QueryInterface failed - %08x\n", hr);
    return;
  }

  // 3. Get the interface to IShellDispatch2
  CComPtr<IDispatch> dispShell;
  hr = folderView->get_Application(&dispShell);
  if (FAILED(hr)) {
    Log(L"IShellFolderViewDual::get_Application failed - %08x\n", hr);
    return;
  }

  CComPtr<IShellDispatch2> shellDisp;
  hr = dispShell->QueryInterface(IID_IShellDispatch2,
                                 reinterpret_cast<void **>(&shellDisp));
  if (FAILED(hr)) {
    Log(L"IDispatch::QueryInterface failed - %08x\n", hr);
    return;
  }

  // Passing the foreground privilege so that the shell can launch an
  // application in the foreground.  If CoAllowSetForegroundWindow fails,
  // we continue because it's not fatal.
  hr = ::CoAllowSetForegroundWindow(shellDisp, nullptr);
  if (FAILED(hr)) {
    Log(L"CoAllowSetForegroundWindow failed - %08x\n", hr);
  }

  // 4. Now call IShellDispatch2::ShellExecute to ask Explorer to execute.
  hr = shellDisp->ShellExecute(aPath, aArgs, aWorkingDir, aVerb, aShowCmd);
  Log(L"IShellDispatch2::ShellExecute returned - %08x\n", hr);
}

#ifndef DOWNLEVEL
void SetProcessMitigationPolicy() {
  PROCESS_MITIGATION_IMAGE_LOAD_POLICY pol = {};
  pol.PreferSystem32Images = 1;
  if (!::SetProcessMitigationPolicy(ProcessImageLoadPolicy, &pol,
                                    sizeof(pol))) {
    Log(L"SetProcessMitigationPolicy failed - %08x\n", GetLastError());
  }
}

class CodeIntegrityGuard final {
  Blob attributeListBlob_;
  LPPROC_THREAD_ATTRIBUTE_LIST attributeList_;
  DWORD64 policy_;

public:
  CodeIntegrityGuard()
      : attributeList_(nullptr),
        policy_(
            PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON) {
    SIZE_T AttributeListSize;
    if (InitializeProcThreadAttributeList(nullptr,
                                          /*dwAttributeCount*/ 1,
                                          /*dwFlags*/ 0, &AttributeListSize) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      return;
    }

    if (!attributeListBlob_.Alloc(AttributeListSize))
      return;

    attributeList_ = attributeListBlob_.As<_PROC_THREAD_ATTRIBUTE_LIST>();
    if (!InitializeProcThreadAttributeList(attributeList_,
                                           /*dwAttributeCount*/ 1,
                                           /*dwFlags*/ 0, &AttributeListSize)) {
      attributeList_ = nullptr;
      Log(L"InitializeProcThreadAttributeList failed - %08x\n", GetLastError());
      return;
    }

    if (!UpdateProcThreadAttribute(attributeList_,
                                   /*dwFlags*/ 0,
                                   PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                                   &policy_, sizeof(policy_),
                                   /*lpPreviousValue*/ nullptr,
                                   /*lpReturnSize*/ nullptr)) {
      Log(L"UpdateProcThreadAttribute failed - %08x\n", GetLastError());
    }
  }

  ~CodeIntegrityGuard() {
    if (attributeList_) {
      DeleteProcThreadAttributeList(attributeList_);
    }
  }

  operator LPPROC_THREAD_ATTRIBUTE_LIST() { return attributeList_; }
};
#endif

struct HandleCloser {
  typedef HANDLE pointer;
  void operator()(HANDLE h) {
    if (h) {
      ::CloseHandle(h);
    }
  }
};

void DoCreateProcess(const wchar_t *executable, const std::wstring &command,
                     DWORD creationFlags, LPSTARTUPINFO si, bool createJob) {
  std::unique_ptr<wchar_t[]> copied(new wchar_t[command.size() + 1]);
  if (!copied) {
    return;
  }

  command.copy(copied.get(), command.size());
  copied[command.size()] = 0;

  // wchar_t env[] = L"PATH=D:\\MSWORK\\pub;%CommonProgramFiles%\\pub\0\0";
  // creationFlags |= CREATE_UNICODE_ENVIRONMENT;
  wchar_t* env = nullptr;

  PROCESS_INFORMATION pi = {};
  if (!CreateProcess(executable, copied.get(),
                     /*lpProcessAttributes*/ nullptr,
                     /*lpThreadAttributes*/ nullptr,
                     /*bInheritHandles*/ FALSE, creationFlags,
                     /*lpEnvironment*/ env,
                     /*lpCurrentDirectory*/ nullptr, si, &pi)) {
    Log(L"CreateProcess failed - %08x\n", GetLastError());
  }

  std::unique_ptr<HANDLE, HandleCloser> job, thread(pi.hThread),
      process(pi.hProcess);

  if (createJob) {
    job.reset(::CreateJobObjectW(/*lpJobAttributes*/ nullptr,
                                 /*lpName*/ nullptr));

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
    jobInfo.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;

    if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                   &jobInfo, sizeof(jobInfo))) {
      Log(L"SetInformationJobObject failed - %08lx\n", ::GetLastError());
    }

    if (!::AssignProcessToJobObject(job.get(), process.get())) {
      Log(L"AssignProcessToJobObject failed - %08lx\n", ::GetLastError());
    }
  }

  WaitForSingleObject(process.get(), INFINITE);
}

int wmain(int argc, wchar_t *argv[]) {
  Args args(argc, argv);
  if (!args)
    return 1;

#ifndef DOWNLEVEL
  if (args.MitigationPolicy())
    SetProcessMitigationPolicy();
#endif

#if 1
  auto env = std::make_unique<wchar_t[]>(50000);
  int i = 0;
  env[i++] = L'@';
  for (i = 1; i < 5000; ++i) { env[i] = L'!'; }
  env[i] = 0;
  i = 1000;
  env[i++] = L'%';
  env[i++] = L'P';
  env[i++] = L'A';
  env[i++] = L'T';
  env[i++] = L'H';
  env[i++] = L'%';

  if (!::SetEnvironmentVariable(L"PATH", env.get())) {
    Log(L"SetEnvironmentVariable failed with %08x\n", ::GetLastError());
    return 1;
  }
#endif

  switch (args.GetMethod()) {
  case Args::Api::ShellExecute: {
    auto commandArgs = args.GetCommandArgs();
    SHELLEXECUTEINFOW seinfo = {sizeof(seinfo)};
    seinfo.fMask = args.Async() ? SEE_MASK_ASYNCOK
                                : (SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI);
    seinfo.lpFile = args.GetExecutable();
    seinfo.lpParameters = commandArgs.c_str();
    seinfo.nShow = SW_SHOWNORMAL;

    BOOL ret = ::ShellExecuteExW(&seinfo);
    Log(L"ShellExecuteExW returned %d hInstApp=%p\n", ret, seinfo.hInstApp);
    if (!ret)
      Log(L"ShellExecuteEx failed with %08x\n", ::GetLastError());
    break;
  }

  case Args::Api::ShellExecuteByExplorer:
    if (SUCCEEDED(CoInitialize(nullptr))) {
      auto commandArgs = args.GetCommandArgs();
      _variant_t varErr(DISP_E_PARAMNOTFOUND, VT_ERROR);

      LaunchViaShell(args.GetExecutable(), commandArgs.c_str(), varErr, varErr,
                     SW_SHOWNORMAL);
    }
    CoUninitialize();
    break;

  case Args::Api::CreateProcess:
#ifndef DOWNLEVEL
    if (args.Cig()) {
      CodeIntegrityGuard cig;

      STARTUPINFOEX sie = {};
      sie.StartupInfo.cb = sizeof(sie);
      sie.lpAttributeList = cig;

      DoCreateProcess(args.GetExecutable(), args.GetFullCommand(),
                      EXTENDED_STARTUPINFO_PRESENT, &sie.StartupInfo,
                      args.Job());
    } else
#endif
    {
      STARTUPINFO si = {sizeof(si)};
      DoCreateProcess(args.GetExecutable(), args.GetFullCommand(), 0, &si,
                      args.Job());
    }
    break;
  }
  return 0;
}
