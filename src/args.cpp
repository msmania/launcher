#include <string>
#include <vector>
#include "args.h"

bool Args::quiet_ = false;

void Args::ShowUsage() {
  if (quiet_) return;
  wprintf(
    L"%sUsage: la [OPTION]... -c [COMMAND]...\n"
    L"\n  --api=[cp|se|shell]   choose a method to launch a process: CreateProcess, ShellExecute,"
    L"\n                        or IShellDispatch2.ShellExecute"
    L"\n  --async               launch a process asynchronously"
    L"\n  --policy              turn on PreferSystem32Images before launching a process"
    L"\n  --cig                 launch a process with CIG"
    L"\n  --job                 launch a process with job"
    L"\n",
    errorMessage_.c_str());
}

bool Args::MatchMethod(const wchar_t *source) {
  auto len = wcslen(source);
  if (len < 5) return false;
  if (wcsncmp(source, L"--api", 5) != 0) return false;

  if (source[5] == 0) {
    errorMessage_ = L"The parameter --api needs a value.\n\n";
    return false;
  }
  else if (source[5] != L'=') {
    return false;
  }

  source += 6;

  if (wcscmp(source, L"cp") == 0)
    api_ = Api::CreateProcess;
  else if (wcscmp(source, L"se") == 0)
    api_ = Api::ShellExecute;
  else if (wcscmp(source, L"shell") == 0)
    api_ = Api::ShellExecuteByExplorer;
  else {
    errorMessage_ = L"Unknown method: `";
    errorMessage_ += source;
    errorMessage_ += L"`\n\n";
    return false;
  }

  return true;
}

Args::Args(int argc, const wchar_t *const argv[]) {
  if (argc == 1) {
    ShowUsage();
    return;
  }

  bool pushToArgs = false;

  for (int i = 1; i < argc; ++i) {
    if (pushToArgs) {
      commands_.push_back(argv[i]);
      continue;
    }

    if (wcscmp(argv[i], L"-c") == 0)
      pushToArgs = true;
    else if (wcscmp(argv[i], L"--async") == 0)
      flags_.Async = 1;
    else if (wcscmp(argv[i], L"--policy") == 0)
      flags_.MitigationPolicy = 1;
    else if (wcscmp(argv[i], L"--cig") == 0)
      flags_.Cig = 1;
    else if (wcscmp(argv[i], L"--job") == 0)
      flags_.Job = 1;
    else if (MatchMethod(argv[i]))
      ;
    else {
      if (errorMessage_.empty()) {
        errorMessage_ = L"Invalid argument: `";
        errorMessage_ += argv[i];
        errorMessage_ += L"`\n\n";
      }
      ShowUsage();
      return;
    }
  }

  if (commands_.size() == 0)
    errorMessage_ = L"Specify a command following the `-c` option.\n\n";
  else if (api_ == Api::CreateProcess && flags_.Async)
    errorMessage_ = L"CreateProcess must be synchronous.\n\n";
  else if (api_ == Api::ShellExecuteByExplorer && !flags_.Async)
    errorMessage_ = L"ShellExecuteByExplorer must be asynchronous.\n\n";
  else if (api_ != Api::CreateProcess && flags_.Job)
    errorMessage_ = L"Job works with CreateProcess only.\n\n";

  if (errorMessage_.empty())
    ok_ = true;
  else
    ShowUsage();
}

const wchar_t *Args::GetExecutable() const {
  return commands_.size() > 0 ? commands_[0] : nullptr;
}

std::wstring Args::GetCommandArgs() const {
  std::wstring ret;
  for (int i = 1; i < commands_.size(); ++i) {
    if (i > 1) ret += ' ';
    ret += commands_[i];
  }
  return ret;
}

std::wstring Args::GetFullCommand() const {
  std::wstring ret;
  for (int i = 0; i < commands_.size(); ++i) {
    if (i > 0) ret += ' ';

    if (wcschr(commands_[i], L' ')) {
      ret += L"\"";
      ret += commands_[i];
      ret += L"\"";
    }
    else {
      ret += commands_[i];
    }
  }
  return ret;
}
