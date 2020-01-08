#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "args.h"

Args MakeArgs(const std::vector<const wchar_t*> &arr) {
  return Args(static_cast<int>(arr.size()), arr.data());
}

TEST(Args, Positive) {
  Args::Quiet();

  // async, cig
  auto args = MakeArgs({L"exepath",
    L"--cig",
    L"--api=shell",
    L"--async",
    L"--policy",
    L"-c", L"--api=command", L"arg1", L"arg2",
  });
  EXPECT_TRUE(args);
  EXPECT_EQ(args.GetMethod(), Args::Api::ShellExecuteByExplorer);
  EXPECT_TRUE(args.Async());
  EXPECT_TRUE(args.Cig());
  EXPECT_STREQ(args.GetExecutable(), L"--api=command");
  EXPECT_STREQ(args.GetCommandArgs().c_str(), L"arg1 arg2");

  // sync, nocig, no args
  args = MakeArgs({L"exepath",
    L"--api=cp",
    L"--policy",
    L"-c", L"--api=command",
  });
  EXPECT_TRUE(args);
  EXPECT_EQ(args.GetMethod(), Args::Api::CreateProcess);
  EXPECT_FALSE(args.Async());
  EXPECT_FALSE(args.Cig());
  EXPECT_STREQ(args.GetExecutable(), L"--api=command");
  EXPECT_STREQ(args.GetCommandArgs().c_str(), L"");
}

TEST(Args, Negative) {
  Args::Quiet();

  // No params
  auto args = MakeArgs({L"exepath"});
  EXPECT_FALSE(args);

  // No command args
  args = MakeArgs({L"exepath",
    L"--cig",
    L"--api=shell",
    L"--async",
    L"--policy",
  });
  EXPECT_FALSE(args);

  // Invalid method
  args = MakeArgs({L"exepath",
    L"--cig",
    L"--api=invalid",
    L"--async",
    L"--policy",
    L"-c", L"--api=command", L"x",
  });
  EXPECT_FALSE(args);

  // Invalid param
  args = MakeArgs({L"exepath",
    L"--cig",
    L"--api=invalid",
    L"--?",
    L"--policy",
    L"-c", L"--api=command", L"x",
  });
  EXPECT_FALSE(args);

  // CreateProcess + async
  args = MakeArgs({L"exepath",
    L"--api=cp",
    L"--async",
    L"-c", L"--api=command", L"x",
  });
  EXPECT_FALSE(args);

  // ShellExecuteByExplorer + sync
  args = MakeArgs({L"exepath",
    L"--api=shell",
    L"-c", L"--api=command", L"x",
  });
  EXPECT_FALSE(args);
}
