#include <stdio.h>
#include <windows.h>
#include <atlbase.h>
#include <comutil.h>
#include <exdisp.h>
#include <shlobj.h>
#include <shobjidl_core.h>
#include <shlwapi.h>
#include <shldisp.h>
#include "blob.h"

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

void LaunchViaShell(const _bstr_t& aPath,
                    const _variant_t& aArgs,
                    const _variant_t& aVerb,
                    const _variant_t& aWorkingDir,
                    const _variant_t& aShowCmd) {
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
                                  SWFO_NEEDDISPATCH,
                                  &dispDesktop);
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
                                   reinterpret_cast<void**>(&servProv));
  if (!servProv) {
    Log(L"QueryInterface(IServiceProvider) failed - %08x\n", hr);
    return;
  }

  CComPtr<IShellBrowser> browser;
  hr = servProv->QueryService(SID_STopLevelBrowser, IID_IShellBrowser,
                              reinterpret_cast<void**>(&browser));
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
                                      reinterpret_cast<void**>(&dispView));
  if (FAILED(hr)) {
    Log(L"IShellView::GetItemObject failed - %08x\n", hr);
    return;
  }

  CComPtr<IShellFolderViewDual> folderView;
  hr = dispView->QueryInterface(IID_IShellFolderViewDual,
                                reinterpret_cast<void**>(&folderView));
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
                                 reinterpret_cast<void**>(&shellDisp));
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

void Launch(LPCWSTR path, bool async) {
  SHELLEXECUTEINFOW seinfo{};
  seinfo.cbSize = sizeof(SHELLEXECUTEINFOW);
  seinfo.fMask = async ? SEE_MASK_ASYNCOK : (SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI);
  seinfo.hwnd = nullptr;
  seinfo.lpVerb = nullptr;
  seinfo.lpFile = path;
  seinfo.lpParameters = nullptr;
  seinfo.lpDirectory = nullptr;
  seinfo.nShow = SW_SHOWNORMAL;

  // Use the directory of the file we're launching as the working
  // directory. That way if we have a self extracting EXE it won't
  // suggest to extract to the install directory.
  WCHAR workingDirectory[MAX_PATH + 1] = {L'\0'};
  wcsncpy(workingDirectory, path, MAX_PATH);
  if (PathRemoveFileSpecW(workingDirectory)) {
    seinfo.lpDirectory = workingDirectory;
  } else {
    Log(L"Could not set working directory for launched file.");
  }

  BOOL ret = ::ShellExecuteExW(&seinfo);
  wprintf(L"ShellExecuteExW returned %d\n", ret);
  wprintf(L"hInstApp %p\n", seinfo.hInstApp);
  if (!ret) {
    wprintf(L"ShellExecuteEx failed with %08x\n", ::GetLastError());
  }
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
      policy_(PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON) {
    SIZE_T AttributeListSize;
    if (InitializeProcThreadAttributeList(nullptr,
                                          /*dwAttributeCount*/1,
                                          /*dwFlags*/0,
                                          &AttributeListSize)
        || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      return;
    }

    if (!attributeListBlob_.Alloc(AttributeListSize)) return;

    attributeList_ = attributeListBlob_.As<_PROC_THREAD_ATTRIBUTE_LIST>();
    if (!InitializeProcThreadAttributeList(attributeList_,
                                           /*dwAttributeCount*/1,
                                           /*dwFlags*/0,
                                           &AttributeListSize)) {
      attributeList_ = nullptr;
      Log(L"InitializeProcThreadAttributeList failed - %08x\n", GetLastError());
      return;
    }


    if (!UpdateProcThreadAttribute(attributeList_,
                                   /*dwFlags*/0,
                                   PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                                   &policy_,
                                   sizeof(policy_),
                                   /*lpPreviousValue*/nullptr,
                                   /*lpReturnSize*/nullptr)) {
      Log(L"UpdateProcThreadAttribute failed - %08x\n", GetLastError());
    }
  }

  ~CodeIntegrityGuard() {
    if (attributeList_) {
      DeleteProcThreadAttributeList(attributeList_);
    }
  }

  operator LPPROC_THREAD_ATTRIBUTE_LIST() {
    return attributeList_;
  }
};

void LaunchWithCIG(LPCWSTR path) {
  CodeIntegrityGuard cig;

  STARTUPINFOEXW StartupInfoEx = {};
  StartupInfoEx.StartupInfo.cb = sizeof(StartupInfoEx);
  StartupInfoEx.lpAttributeList = cig;

  size_t len = wcslen(path) + 1;
  if (auto copied = new wchar_t[len]) {
    memcpy(copied, path, len * sizeof(wchar_t));

    PROCESS_INFORMATION ProcessInformation = {};
    if (!CreateProcess(nullptr,
                       copied,
                       /*lpProcessAttributes*/nullptr,
                       /*lpThreadAttributes*/nullptr,
                       /*bInheritHandles*/FALSE,
                       EXTENDED_STARTUPINFO_PRESENT,
                       /*lpEnvironment*/nullptr,
                       /*lpCurrentDirectory*/nullptr,
                       &StartupInfoEx.StartupInfo,
                       &ProcessInformation)) {
      Log(L"CreateProcess failed - %08x\n", GetLastError());
    }

    WaitForSingleObject(ProcessInformation.hProcess, INFINITE);
    CloseHandle(ProcessInformation.hThread);
    CloseHandle(ProcessInformation.hProcess);

    delete [] copied;
  }
}
#endif

void ShowUsage(LPCWSTR path) {
  wprintf(L"Usage: %s [-m|-s|-a|-g] <path>\n", path);
}

int wmain(int argc, wchar_t* argv[]) {
  if (argc < 2) {
    ShowUsage(argv[0]);
    return 1;
  }

  if (argc == 2) {
    Launch(argv[1], /*async*/false);
  }
  else if (wcscmp(argv[1], L"-a") == 0) {
    Launch(argv[2], /*async*/true);
    wprintf(L"Hit any key to finish...\n");
    getchar();
  }
  else if (wcscmp(argv[1], L"-s") == 0) {
#ifndef DOWNLEVEL
    SetProcessMitigationPolicy();
#endif
    if (SUCCEEDED(::CoInitialize(nullptr))) {
      if (wcscmp(argv[2], L"null") == 0) {
        _variant_t varErr(DISP_E_PARAMNOTFOUND, VT_ERROR);
        LaunchViaShell(argv[3], L"", varErr, L"C:\\mswork", SW_SHOWNORMAL);
      }
      else {
        _variant_t verb(argv[2]);
        LaunchViaShell(argv[3], L"", verb, L"C:\\mswork", SW_SHOWNORMAL);
      }
      CoUninitialize();
    }
  }
#ifndef DOWNLEVEL
  else if (wcscmp(argv[1], L"-m") == 0) {
    SetProcessMitigationPolicy();
    Launch(argv[2], /*async*/true);
    wprintf(L"Hit any key to finish...\n");
    getchar();
  }
  else if (wcscmp(argv[1], L"-g") == 0) {
    LaunchWithCIG(argv[2]);
  }
#endif
  else {
    ShowUsage(argv[0]);
    return 1;
  }

  return 0;
}