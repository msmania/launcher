#pragma once

class Args {
  static bool quiet_;

public:
  enum class Api {
    Uninitialized,
    CreateProcess,
    ShellExecute,
    ShellExecuteByExplorer
  };

private:
  Api api_ = Api::Uninitialized;

  union {
    struct {
      uint32_t Async : 1;
      uint32_t MitigationPolicy : 1;
      uint32_t Cig : 1;
      uint32_t Job : 1;
    } flags_;
    uint32_t all_ = 0;
  };

  std::vector<const wchar_t*> commands_;

  std::wstring errorMessage_;
  bool ok_ = false;

  void ShowUsage();
  bool MatchMethod(const wchar_t *source);

public:
  static void Quiet() {quiet_ = true;}
  Args(int argc, const wchar_t *const argv[]);
  operator bool() const {return ok_;}

  Api GetMethod() const {return api_;};
  bool Async() const {return flags_.Async;};
  bool MitigationPolicy() const {return flags_.MitigationPolicy;};
  bool Cig() const {return flags_.Cig;};
  bool Job() const {return flags_.Job;};

  const wchar_t *GetExecutable() const;
  std::wstring GetCommandArgs() const;
  std::wstring GetFullCommand() const;
};
