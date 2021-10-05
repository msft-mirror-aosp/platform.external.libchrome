// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher.h"

#include <stddef.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/launcher/test_launcher_test_utils.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace base {
namespace {

TestResult GenerateTestResult(const std::string& test_name,
                              TestResult::Status status,
                              TimeDelta elapsed_td = Milliseconds(30),
                              const std::string& output_snippet = "output") {
  TestResult result;
  result.full_name = test_name;
  result.status = status;
  result.elapsed_time = elapsed_td;
  result.output_snippet = output_snippet;
  return result;
}

TestResultPart GenerateTestResultPart(TestResultPart::Type type,
                                      const std::string& file_name,
                                      int line_number,
                                      const std::string& summary,
                                      const std::string& message) {
  TestResultPart test_result_part;
  test_result_part.type = type;
  test_result_part.file_name = file_name;
  test_result_part.line_number = line_number;
  test_result_part.summary = summary;
  test_result_part.message = message;
  return test_result_part;
}

// Mock TestLauncher to mock CreateAndStartThreadPool,
// unit test will provide a TaskEnvironment.
class MockTestLauncher : public TestLauncher {
 public:
  MockTestLauncher(TestLauncherDelegate* launcher_delegate,
                   size_t parallel_jobs)
      : TestLauncher(launcher_delegate, parallel_jobs) {}

  void CreateAndStartThreadPool(int parallel_jobs) override {}

  MOCK_METHOD4(LaunchChildGTestProcess,
               void(scoped_refptr<TaskRunner> task_runner,
                    const std::vector<std::string>& test_names,
                    const FilePath& task_temp_dir,
                    const FilePath& child_temp_dir));
};

// Simple TestLauncherDelegate mock to test TestLauncher flow.
class MockTestLauncherDelegate : public TestLauncherDelegate {
 public:
  MOCK_METHOD1(GetTests, bool(std::vector<TestIdentifier>* output));
  MOCK_METHOD2(WillRunTest,
               bool(const std::string& test_case_name,
                    const std::string& test_name));
  MOCK_METHOD2(ProcessTestResults,
               void(std::vector<TestResult>& test_names,
                    TimeDelta elapsed_time));
  MOCK_METHOD3(GetCommandLine,
               CommandLine(const std::vector<std::string>& test_names,
                           const FilePath& temp_dir_,
                           FilePath* output_file_));
  MOCK_METHOD1(IsPreTask, bool(const std::vector<std::string>& test_names));
  MOCK_METHOD0(GetWrapper, std::string());
  MOCK_METHOD0(GetLaunchOptions, int());
  MOCK_METHOD0(GetTimeout, TimeDelta());
  MOCK_METHOD0(GetBatchSize, size_t());
};

// Using MockTestLauncher to test TestLauncher.
// Test TestLauncher filters, and command line switches setup.
class TestLauncherTest : public testing::Test {
 protected:
  TestLauncherTest()
      : command_line(new CommandLine(CommandLine::NO_PROGRAM)),
        test_launcher(&delegate, 10),
        task_environment(base::test::TaskEnvironment::MainThreadType::IO) {}

  // Adds tests to be returned by the delegate.
  void AddMockedTests(std::string test_case_name,
                      const std::vector<std::string>& test_names) {
    for (const std::string& test_name : test_names) {
      TestIdentifier test_data;
      test_data.test_case_name = test_case_name;
      test_data.test_name = test_name;
      test_data.file = "File";
      test_data.line = 100;
      tests_.push_back(test_data);
    }
  }

  // Setup expected delegate calls, and which tests the delegate will return.
  void SetUpExpectCalls(size_t batch_size = 10) {
    using ::testing::_;
    EXPECT_CALL(delegate, GetTests(_))
        .WillOnce(::testing::DoAll(testing::SetArgPointee<0>(tests_),
                                   testing::Return(true)));
    EXPECT_CALL(delegate, WillRunTest(_, _))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(delegate, ProcessTestResults(_, _)).Times(0);
    EXPECT_CALL(delegate, GetCommandLine(_, _, _))
        .WillRepeatedly(testing::Return(CommandLine(CommandLine::NO_PROGRAM)));
    EXPECT_CALL(delegate, GetWrapper())
        .WillRepeatedly(testing::Return(std::string()));
    EXPECT_CALL(delegate, IsPreTask(_)).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(delegate, GetLaunchOptions())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(delegate, GetTimeout())
        .WillRepeatedly(testing::Return(TimeDelta()));
    EXPECT_CALL(delegate, GetBatchSize())
        .WillRepeatedly(testing::Return(batch_size));
  }

  std::unique_ptr<CommandLine> command_line;
  MockTestLauncher test_launcher;
  MockTestLauncherDelegate delegate;
  base::test::TaskEnvironment task_environment;
  ScopedTempDir dir;

 private:
  std::vector<TestIdentifier> tests_;
};

// Action to mock delegate invoking OnTestFinish on test launcher.
ACTION_P3(OnTestResult, launcher, full_name, status) {
  TestResult result = GenerateTestResult(full_name, status);
  arg0->PostTask(FROM_HERE, BindOnce(&TestLauncher::OnTestFinished,
                                     Unretained(launcher), result));
}

// Action to mock delegate invoking OnTestFinish on test launcher.
ACTION_P2(OnTestResult, launcher, result) {
  arg0->PostTask(FROM_HERE, BindOnce(&TestLauncher::OnTestFinished,
                                     Unretained(launcher), result));
}

// A test and a disabled test cannot share a name.
TEST_F(TestLauncherTest, TestNameSharedWithDisabledTest) {
  AddMockedTests("Test", {"firstTest", "DISABLED_firstTest"});
  SetUpExpectCalls();
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// A test case and a disabled test case cannot share a name.
TEST_F(TestLauncherTest, TestNameSharedWithDisabledTestCase) {
  AddMockedTests("DISABLED_Test", {"firstTest"});
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Compiled tests should not contain an orphaned pre test.
TEST_F(TestLauncherTest, OrphanePreTest) {
  AddMockedTests("Test", {"firstTest", "PRE_firstTestOrphane"});
  SetUpExpectCalls();
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// When There are no tests, delegate should not be called.
TEST_F(TestLauncherTest, EmptyTestSetPasses) {
  SetUpExpectCalls();
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _)).Times(0);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher filters DISABLED tests by default.
TEST_F(TestLauncherTest, FilterDisabledTestByDefault) {
  AddMockedTests("DISABLED_TestDisabled", {"firstTest"});
  AddMockedTests("Test",
                 {"firstTest", "secondTest", "DISABLED_firstTestDisabled"});
  SetUpExpectCalls();
  std::vector<std::string> tests_names = {"Test.firstTest", "Test.secondTest"};
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_SUCCESS),
                                 OnTestResult(&test_launcher, "Test.secondTest",
                                              TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher should reorder PRE_ tests before delegate
TEST_F(TestLauncherTest, ReorderPreTests) {
  AddMockedTests("Test", {"firstTest", "PRE_PRE_firstTest", "PRE_firstTest"});
  SetUpExpectCalls();
  std::vector<std::string> tests_names = {
      "Test.PRE_PRE_firstTest", "Test.PRE_firstTest", "Test.firstTest"};
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher "gtest_filter" switch.
TEST_F(TestLauncherTest, UsingCommandLineFilter) {
  AddMockedTests("Test",
                 {"firstTest", "secondTest", "DISABLED_firstTestDisabled"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("gtest_filter", "Test*.first*");
  std::vector<std::string> tests_names = {"Test.firstTest"};
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher gtest filter will include pre tests
TEST_F(TestLauncherTest, FilterIncludePreTest) {
  AddMockedTests("Test", {"firstTest", "secondTest", "PRE_firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("gtest_filter", "Test.firstTest");
  std::vector<std::string> tests_names = {"Test.PRE_firstTest",
                                          "Test.firstTest"};
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher gtest filter works when both include and exclude filter
// are defined.
TEST_F(TestLauncherTest, FilterIncludeExclude) {
  AddMockedTests("Test", {"firstTest", "PRE_firstTest", "secondTest",
                          "PRE_secondTest", "thirdTest", "DISABLED_Disable1"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("gtest_filter",
                                  "Test.*Test:-Test.secondTest");
  std::vector<std::string> tests_names = {
      "Test.PRE_firstTest",
      "Test.firstTest",
      "Test.thirdTest",
  };
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher "gtest_repeat" switch.
TEST_F(TestLauncherTest, RepeatTest) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  // Unless --gtest-break-on-failure is specified,
  command_line->AppendSwitchASCII("gtest_repeat", "2");
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .Times(2)
      .WillRepeatedly(::testing::DoAll(OnTestResult(
          &test_launcher, "Test.firstTest", TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher --gtest_repeat and --gtest_break_on_failure.
TEST_F(TestLauncherTest, RunningMultipleIterationsUntilFailure) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  // Unless --gtest-break-on-failure is specified,
  command_line->AppendSwitchASCII("gtest_repeat", "4");
  command_line->AppendSwitch("gtest_break_on_failure");
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_SUCCESS)))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_SUCCESS)))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_FAILURE)));
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher will retry failed test, and stop on success.
TEST_F(TestLauncherTest, SuccessOnRetryTests) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-retry-limit", "2");
  using ::testing::_;
  std::vector<std::string> tests_names = {"Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_FAILURE))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher will retry continuing failing test up to retry limit,
// before eventually failing and returning false.
TEST_F(TestLauncherTest, FailOnRetryTests) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-retry-limit", "2");
  using ::testing::_;
  std::vector<std::string> tests_names = {"Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(3)
      .WillRepeatedly(OnTestResult(&test_launcher, "Test.firstTest",
                                   TestResult::TEST_FAILURE));
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher should retry all PRE_ chained tests
TEST_F(TestLauncherTest, RetryPreTests) {
  AddMockedTests("Test", {"firstTest", "PRE_PRE_firstTest", "PRE_firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-retry-limit", "2");
  std::vector<TestResult> results = {
      GenerateTestResult("Test.PRE_PRE_firstTest", TestResult::TEST_SUCCESS),
      GenerateTestResult("Test.PRE_firstTest", TestResult::TEST_FAILURE),
      GenerateTestResult("Test.firstTest", TestResult::TEST_SUCCESS)};
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(::testing::DoAll(
          OnTestResult(&test_launcher, "Test.PRE_PRE_firstTest",
                       TestResult::TEST_SUCCESS),
          OnTestResult(&test_launcher, "Test.PRE_firstTest",
                       TestResult::TEST_FAILURE),
          OnTestResult(&test_launcher, "Test.firstTest",
                       TestResult::TEST_SUCCESS)));
  std::vector<std::string> tests_names = {"Test.PRE_PRE_firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.PRE_PRE_firstTest",
                             TestResult::TEST_SUCCESS));
  tests_names = {"Test.PRE_firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.PRE_firstTest",
                             TestResult::TEST_SUCCESS));
  tests_names = {"Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher should fail if a PRE test fails but its non-PRE test passes
TEST_F(TestLauncherTest, PreTestFailure) {
  AddMockedTests("Test", {"FirstTest", "PRE_FirstTest"});
  SetUpExpectCalls();
  std::vector<TestResult> results = {
      GenerateTestResult("Test.PRE_FirstTest", TestResult::TEST_FAILURE),
      GenerateTestResult("Test.FirstTest", TestResult::TEST_SUCCESS)};
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(
          ::testing::DoAll(OnTestResult(&test_launcher, "Test.PRE_FirstTest",
                                        TestResult::TEST_FAILURE),
                           OnTestResult(&test_launcher, "Test.FirstTest",
                                        TestResult::TEST_SUCCESS)));
  EXPECT_CALL(test_launcher,
              LaunchChildGTestProcess(
                  _, testing::ElementsAre("Test.PRE_FirstTest"), _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.PRE_FirstTest",
                             TestResult::TEST_FAILURE));
  EXPECT_CALL(
      test_launcher,
      LaunchChildGTestProcess(_, testing::ElementsAre("Test.FirstTest"), _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.FirstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher run disabled unit tests switch.
TEST_F(TestLauncherTest, RunDisabledTests) {
  AddMockedTests("DISABLED_TestDisabled", {"firstTest"});
  AddMockedTests("Test",
                 {"firstTest", "secondTest", "DISABLED_firstTestDisabled"});
  SetUpExpectCalls();
  command_line->AppendSwitch("gtest_also_run_disabled_tests");
  command_line->AppendSwitchASCII("gtest_filter", "Test*.first*");
  std::vector<std::string> tests_names = {"DISABLED_TestDisabled.firstTest",
                                          "Test.firstTest",
                                          "Test.DISABLED_firstTestDisabled"};
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(::testing::DoAll(
          OnTestResult(&test_launcher, "Test.firstTest",
                       TestResult::TEST_SUCCESS),
          OnTestResult(&test_launcher, "DISABLED_TestDisabled.firstTest",
                       TestResult::TEST_SUCCESS),
          OnTestResult(&test_launcher, "Test.DISABLED_firstTestDisabled",
                       TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Disabled test should disable all pre tests
TEST_F(TestLauncherTest, DisablePreTests) {
  AddMockedTests("Test", {"DISABLED_firstTest", "PRE_PRE_firstTest",
                          "PRE_firstTest", "secondTest"});
  SetUpExpectCalls();
  std::vector<std::string> tests_names = {"Test.secondTest"};
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Tests fail if they produce too much output.
TEST_F(TestLauncherTest, ExcessiveOutput) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  using ::testing::_;
  command_line->AppendSwitchASCII("test-launcher-retry-limit", "0");
  command_line->AppendSwitchASCII("test-launcher-print-test-stdio", "never");
  TestResult test_result =
      GenerateTestResult("Test.firstTest", TestResult::TEST_SUCCESS,
                         Milliseconds(30), std::string(500000, 'a'));
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(OnTestResult(&test_launcher, test_result));
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Use command-line switch to allow more output.
TEST_F(TestLauncherTest, OutputLimitSwitch) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-print-test-stdio", "never");
  command_line->AppendSwitchASCII("test-launcher-output-bytes-limit", "800000");
  using ::testing::_;
  TestResult test_result =
      GenerateTestResult("Test.firstTest", TestResult::TEST_SUCCESS,
                         Milliseconds(30), std::string(500000, 'a'));
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(OnTestResult(&test_launcher, test_result));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Shard index must be lesser than total shards
TEST_F(TestLauncherTest, FaultyShardSetup) {
  command_line->AppendSwitchASCII("test-launcher-total-shards", "2");
  command_line->AppendSwitchASCII("test-launcher-shard-index", "2");
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Shard index must be lesser than total shards
TEST_F(TestLauncherTest, RedirectStdio) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-print-test-stdio", "always");
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Sharding should be stable and always selecting the same tests.
TEST_F(TestLauncherTest, StableSharding) {
  AddMockedTests("Test", {"firstTest", "secondTest", "thirdTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-total-shards", "2");
  command_line->AppendSwitchASCII("test-launcher-shard-index", "0");
  command_line->AppendSwitch("test-launcher-stable-sharding");
  std::vector<std::string> tests_names = {"Test.firstTest", "Test.secondTest"};
  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_SUCCESS),
                                 OnTestResult(&test_launcher, "Test.secondTest",
                                              TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Validate |iteration_data| contains one test result matching |result|.
bool ValidateTestResultObject(const Value* iteration_data,
                              TestResult& test_result) {
  const Value* results = iteration_data->FindListKey(test_result.full_name);
  if (!results) {
    ADD_FAILURE() << "Results not found";
    return false;
  }
  if (1u != results->GetList().size()) {
    ADD_FAILURE() << "Expected one result, actual: "
                  << results->GetList().size();
    return false;
  }
  const Value& val = results->GetList()[0];
  if (!val.is_dict()) {
    ADD_FAILURE() << "Unexpected type";
    return false;
  }

  using test_launcher_utils::ValidateKeyValue;
  bool result = ValidateKeyValue(val, "elapsed_time_ms",
                                 test_result.elapsed_time.InMilliseconds());

  if (!val.FindBoolKey("losless_snippet").value_or(false)) {
    ADD_FAILURE() << "losless_snippet expected to be true";
    result = false;
  }

  result &= ValidateKeyValue(val, "output_snippet", test_result.output_snippet);

  std::string base64_output_snippet;
  Base64Encode(test_result.output_snippet, &base64_output_snippet);
  result &=
      ValidateKeyValue(val, "output_snippet_base64", base64_output_snippet);

  result &= ValidateKeyValue(val, "status", test_result.StatusAsString());

  const Value* value = val.FindListKey("result_parts");
  if (test_result.test_result_parts.size() != value->GetList().size()) {
    ADD_FAILURE() << "test_result_parts count is not valid";
    return false;
  }

  for (unsigned i = 0; i < test_result.test_result_parts.size(); i++) {
    TestResultPart result_part = test_result.test_result_parts.at(i);
    const Value& part_dict = value->GetList()[i];

    result &= ValidateKeyValue(part_dict, "type", result_part.TypeAsString());
    result &= ValidateKeyValue(part_dict, "file", result_part.file_name);
    result &= ValidateKeyValue(part_dict, "line", result_part.line_number);
    result &= ValidateKeyValue(part_dict, "summary", result_part.summary);
    result &= ValidateKeyValue(part_dict, "message", result_part.message);
  }
  return result;
}

// Validate |root| dictionary value contains a list with |values|
// at |key| value.
bool ValidateStringList(const absl::optional<Value>& root,
                        const std::string& key,
                        std::vector<const char*> values) {
  const Value* val = root->FindListKey(key);
  if (!val) {
    ADD_FAILURE() << "|root| has no list_value in key: " << key;
    return false;
  }

  if (values.size() != val->GetList().size()) {
    ADD_FAILURE() << "expected size: " << values.size()
                  << ", actual size:" << val->GetList().size();
    return false;
  }

  for (unsigned i = 0; i < values.size(); i++) {
    if (!val->GetList()[i].is_string() &&
        val->GetList()[i].GetString().compare(values.at(i))) {
      ADD_FAILURE() << "Expected list values do not match actual list";
      return false;
    }
  }
  return true;
}

// Unit tests to validate TestLauncher outputs the correct JSON file.
TEST_F(TestLauncherTest, JsonSummary) {
  AddMockedTests("DISABLED_TestDisabled", {"firstTest"});
  AddMockedTests("Test",
                 {"firstTest", "secondTest", "DISABLED_firstTestDisabled"});
  SetUpExpectCalls();

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.json");
  command_line->AppendSwitchPath("test-launcher-summary-output", path);
  command_line->AppendSwitchASCII("gtest_repeat", "2");
  // Force the repeats to run sequentially.
  command_line->AppendSwitch("gtest_break_on_failure");

  // Setup results to be returned by the test launcher delegate.
  TestResult first_result =
      GenerateTestResult("Test.firstTest", TestResult::TEST_SUCCESS,
                         Milliseconds(30), "output_first");
  first_result.test_result_parts.push_back(GenerateTestResultPart(
      TestResultPart::kSuccess, "TestFile", 110, "summary", "message"));
  TestResult second_result =
      GenerateTestResult("Test.secondTest", TestResult::TEST_SUCCESS,
                         Milliseconds(50), "output_second");

  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .Times(2)
      .WillRepeatedly(
          ::testing::DoAll(OnTestResult(&test_launcher, first_result),
                           OnTestResult(&test_launcher, second_result)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));

  // Validate the resulting JSON file is the expected output.
  absl::optional<Value> root = test_launcher_utils::ReadSummary(path);
  ASSERT_TRUE(root);
  EXPECT_TRUE(
      ValidateStringList(root, "all_tests",
                         {"Test.firstTest", "Test.firstTestDisabled",
                          "Test.secondTest", "TestDisabled.firstTest"}));
  EXPECT_TRUE(
      ValidateStringList(root, "disabled_tests",
                         {"Test.firstTestDisabled", "TestDisabled.firstTest"}));

  const Value* val = root->FindDictKey("test_locations");
  ASSERT_TRUE(val);
  EXPECT_EQ(2u, val->DictSize());
  ASSERT_TRUE(test_launcher_utils::ValidateTestLocation(val, "Test.firstTest",
                                                        "File", 100));
  ASSERT_TRUE(test_launcher_utils::ValidateTestLocation(val, "Test.secondTest",
                                                        "File", 100));

  val = root->FindListKey("per_iteration_data");
  ASSERT_TRUE(val);
  ASSERT_EQ(2u, val->GetList().size());
  for (size_t i = 0; i < val->GetList().size(); i++) {
    const Value* iteration_val = &(val->GetList()[i]);
    ASSERT_TRUE(iteration_val);
    ASSERT_TRUE(iteration_val->is_dict());
    EXPECT_EQ(2u, iteration_val->DictSize());
    EXPECT_TRUE(ValidateTestResultObject(iteration_val, first_result));
    EXPECT_TRUE(ValidateTestResultObject(iteration_val, second_result));
  }
}

// Validate TestLauncher outputs the correct JSON file
// when running disabled tests.
TEST_F(TestLauncherTest, JsonSummaryWithDisabledTests) {
  AddMockedTests("Test", {"DISABLED_Test"});
  SetUpExpectCalls();

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.json");
  command_line->AppendSwitchPath("test-launcher-summary-output", path);
  command_line->AppendSwitch("gtest_also_run_disabled_tests");

  // Setup results to be returned by the test launcher delegate.
  TestResult test_result =
      GenerateTestResult("Test.DISABLED_Test", TestResult::TEST_SUCCESS,
                         Milliseconds(50), "output_second");

  using ::testing::_;
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(OnTestResult(&test_launcher, test_result));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));

  // Validate the resulting JSON file is the expected output.
  absl::optional<Value> root = test_launcher_utils::ReadSummary(path);
  ASSERT_TRUE(root);
  Value* val = root->FindDictKey("test_locations");
  ASSERT_TRUE(val);
  EXPECT_EQ(1u, val->DictSize());
  EXPECT_TRUE(test_launcher_utils::ValidateTestLocation(
      val, "Test.DISABLED_Test", "File", 100));

  val = root->FindListKey("per_iteration_data");
  ASSERT_TRUE(val);
  ASSERT_EQ(1u, val->GetList().size());

  Value* iteration_val = &(val->GetList()[0]);
  ASSERT_TRUE(iteration_val);
  ASSERT_TRUE(iteration_val->is_dict());
  EXPECT_EQ(1u, iteration_val->DictSize());
  // We expect the result to be stripped of disabled prefix.
  test_result.full_name = "Test.Test";
  EXPECT_TRUE(ValidateTestResultObject(iteration_val, test_result));
}

// Matches a std::tuple<const FilePath&, const FilePath&> where the first
// item is a parent of the second.
MATCHER(DirectoryIsParentOf, "") {
  return std::get<0>(arg).IsParent(std::get<1>(arg));
}

// Test that the launcher creates a dedicated temp dir for a child proc and
// cleans it up.
TEST_F(TestLauncherTest, TestChildTempDir) {
  using ::testing::_;
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  ON_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillByDefault(OnTestResult(&test_launcher, "Test.firstTest",
                                  TestResult::TEST_SUCCESS));

  FilePath task_temp;
  if (TestLauncher::SupportsPerChildTempDirs()) {
    // Platforms that support child proc temp dirs must get a |child_temp_dir|
    // arg that exists and is within |task_temp_dir|.
    EXPECT_CALL(
        test_launcher,
        LaunchChildGTestProcess(
            _, _, _, ::testing::ResultOf(DirectoryExists, ::testing::IsTrue())))
        .With(::testing::Args<2, 3>(DirectoryIsParentOf()))
        .WillOnce(::testing::SaveArg<2>(&task_temp));
  } else {
    // Platforms that don't support child proc temp dirs must get an empty
    // |child_temp_dir| arg.
    EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, FilePath()))
        .WillOnce(::testing::SaveArg<2>(&task_temp));
  }

  EXPECT_TRUE(test_launcher.Run(command_line.get()));

  // The task's temporary directory should have been deleted.
  EXPECT_FALSE(DirectoryExists(task_temp));
}

#if defined(OS_FUCHSIA)
// Verifies that test processes have /data, /cache and /tmp available.
TEST_F(TestLauncherTest, ProvidesDataCacheAndTmpDirs) {
  EXPECT_TRUE(base::DirectoryExists(base::FilePath("/data")));
  EXPECT_TRUE(base::DirectoryExists(base::FilePath("/cache")));
  EXPECT_TRUE(base::DirectoryExists(base::FilePath("/tmp")));
}
#endif  // defined(OS_FUCHSIA)

// Unit tests to validate UnitTestLauncherDelegate implementation.
class UnitTestLauncherDelegateTester : public testing::Test {
 protected:
  DefaultUnitTestPlatformDelegate defaultPlatform;
  ScopedTempDir dir;

 private:
  base::test::TaskEnvironment task_environment;
};

// Validate delegate produces correct command line.
TEST_F(UnitTestLauncherDelegateTester, GetCommandLine) {
  UnitTestLauncherDelegate launcher_delegate(&defaultPlatform, 10u, true);
  TestLauncherDelegate* delegate_ptr = &launcher_delegate;

  std::vector<std::string> test_names(5, "Tests");
  base::FilePath temp_dir;
  base::FilePath result_file;
  CreateNewTempDirectory(FilePath::StringType(), &temp_dir);

  CommandLine cmd_line =
      delegate_ptr->GetCommandLine(test_names, temp_dir, &result_file);
  EXPECT_TRUE(cmd_line.HasSwitch("single-process-tests"));
  EXPECT_EQ(cmd_line.GetSwitchValuePath("test-launcher-output"), result_file);

  const int size = 2048;
  std::string content;
  ASSERT_TRUE(ReadFileToStringWithMaxSize(
      cmd_line.GetSwitchValuePath("gtest_flagfile"), &content, size));
  EXPECT_EQ(content.find("--gtest_filter="), 0u);
  base::ReplaceSubstringsAfterOffset(&content, 0, "--gtest_filter=", "");
  std::vector<std::string> gtest_filter_tests =
      SplitString(content, ":", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(gtest_filter_tests.size(), test_names.size());
  for (unsigned i = 0; i < test_names.size(); i++) {
    EXPECT_EQ(gtest_filter_tests.at(i), test_names.at(i));
  }
}

// Validate delegate sets batch size correctly.
TEST_F(UnitTestLauncherDelegateTester, BatchSize) {
  UnitTestLauncherDelegate launcher_delegate(&defaultPlatform, 15u, true);
  TestLauncherDelegate* delegate_ptr = &launcher_delegate;
  EXPECT_EQ(delegate_ptr->GetBatchSize(), 15u);
}

// The following 3 tests are disabled as they are meant to only run from
// |RunMockTests| to validate tests launcher output for known results.

// Basic test to pass
TEST(MockUnitTests, DISABLED_PassTest) {
  ASSERT_TRUE(true);
}
// Basic test to fail
TEST(MockUnitTests, DISABLED_FailTest) {
  ASSERT_TRUE(false);
}
// Basic test to crash
TEST(MockUnitTests, DISABLED_CrashTest) {
  IMMEDIATE_CRASH();
}
// Basic test will not be reached, due to the preceding crash in the same batch.
TEST(MockUnitTests, DISABLED_NoRunTest) {
  ASSERT_TRUE(true);
}

// Using TestLauncher to launch 3 basic unitests
// and validate the resulting json file.
TEST_F(UnitTestLauncherDelegateTester, RunMockTests) {
  CommandLine command_line(CommandLine::ForCurrentProcess()->GetProgram());
  command_line.AppendSwitchASCII("gtest_filter", "MockUnitTests.DISABLED_*");

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.json");
  command_line.AppendSwitchPath("test-launcher-summary-output", path);
  command_line.AppendSwitch("gtest_also_run_disabled_tests");
  command_line.AppendSwitchASCII("test-launcher-retry-limit", "0");
#if defined(OS_WIN)
  // In Windows versions prior to Windows 8, nested job objects are
  // not allowed and cause this test to fail.
  if (win::GetVersion() < win::Version::WIN8) {
    command_line.AppendSwitch(kDontUseJobObjectFlag);
  }
#endif  // defined(OS_WIN)

  std::string output;
  GetAppOutputAndError(command_line, &output);

  // Validate the resulting JSON file is the expected output.
  absl::optional<Value> root = test_launcher_utils::ReadSummary(path);
  ASSERT_TRUE(root);

  Value* val = root->FindDictKey("test_locations");
  ASSERT_TRUE(val);
  EXPECT_EQ(4u, val->DictSize());

  EXPECT_TRUE(test_launcher_utils::ValidateTestLocations(val, "MockUnitTests"));

  val = root->FindListKey("per_iteration_data");
  ASSERT_TRUE(val);
  ASSERT_EQ(1u, val->GetList().size());

  Value* iteration_val = &(val->GetList()[0]);
  ASSERT_TRUE(iteration_val);
  ASSERT_TRUE(iteration_val->is_dict());
  EXPECT_EQ(4u, iteration_val->DictSize());
  // We expect the result to be stripped of disabled prefix.
  EXPECT_TRUE(test_launcher_utils::ValidateTestResult(
      iteration_val, "MockUnitTests.PassTest", "SUCCESS", 0u));
  EXPECT_TRUE(test_launcher_utils::ValidateTestResult(
      iteration_val, "MockUnitTests.FailTest", "FAILURE", 1u));
  EXPECT_TRUE(test_launcher_utils::ValidateTestResult(
      iteration_val, "MockUnitTests.CrashTest", "CRASH", 0u));
  EXPECT_TRUE(test_launcher_utils::ValidateTestResult(
      iteration_val, "MockUnitTests.NoRunTest", "NOTRUN", 0u));
}

// TODO(crbug.com/1094369): Enable leaked-child checks on other platforms.
#if defined(OS_FUCHSIA)

// Test that leaves a child process running. The test is DISABLED_, so it can
// be launched explicitly by RunMockLeakProcessTest

MULTIPROCESS_TEST_MAIN(LeakChildProcess) {
  while (true)
    PlatformThread::Sleep(base::Seconds(1));
}

TEST(LeakedChildProcessTest, DISABLED_LeakChildProcess) {
  Process child_process = SpawnMultiProcessTestChild(
      "LeakChildProcess", GetMultiProcessTestChildBaseCommandLine(),
      LaunchOptions());
  ASSERT_TRUE(child_process.IsValid());
  // Don't wait for the child process to exit.
}

// Validate that a test that leaks a process causes the batch to have an
// error exit_code.
TEST_F(UnitTestLauncherDelegateTester, LeakedChildProcess) {
  CommandLine command_line(CommandLine::ForCurrentProcess()->GetProgram());
  command_line.AppendSwitchASCII(
      "gtest_filter", "LeakedChildProcessTest.DISABLED_LeakChildProcess");

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.json");
  command_line.AppendSwitchPath("test-launcher-summary-output", path);
  command_line.AppendSwitch("gtest_also_run_disabled_tests");
  command_line.AppendSwitchASCII("test-launcher-retry-limit", "0");
#if defined(OS_WIN)
  // In Windows versions prior to Windows 8, nested job objects are
  // not allowed and cause this test to fail.
  if (win::GetVersion() < win::Version::WIN8) {
    command_line.AppendSwitch(kDontUseJobObjectFlag);
  }
#endif  // defined(OS_WIN)

  std::string output;
  int exit_code = 0;
  GetAppOutputWithExitCode(command_line, &output, &exit_code);

  // Validate that we actually ran a test.
  absl::optional<Value> root = test_launcher_utils::ReadSummary(path);
  ASSERT_TRUE(root);

  Value* val = root->FindDictKey("test_locations");
  ASSERT_TRUE(val);
  EXPECT_EQ(1u, val->DictSize());

  EXPECT_TRUE(test_launcher_utils::ValidateTestLocations(
      val, "LeakedChildProcessTest"));

  // Validate that the leaked child caused the batch to error-out.
  EXPECT_EQ(exit_code, 1);
}
#endif  // defined(OS_FUCHSIA)

// Validate GetTestOutputSnippetTest assigns correct output snippet.
TEST(TestLauncherTools, GetTestOutputSnippetTest) {
  const std::string output =
      "[ RUN      ] TestCase.FirstTest\n"
      "[       OK ] TestCase.FirstTest (0 ms)\n"
      "Post first test output\n"
      "[ RUN      ] TestCase.SecondTest\n"
      "[  FAILED  ] TestCase.SecondTest (0 ms)\n"
      "[ RUN      ] TestCase.ThirdTest\n"
      "[  SKIPPED ] TestCase.ThirdTest (0 ms)\n"
      "Post second test output";
  TestResult result;

  // test snippet of a successful test
  result.full_name = "TestCase.FirstTest";
  result.status = TestResult::TEST_SUCCESS;
  EXPECT_EQ(GetTestOutputSnippet(result, output),
            "[ RUN      ] TestCase.FirstTest\n"
            "[       OK ] TestCase.FirstTest (0 ms)\n");

  // test snippet of a failure on exit tests should include output
  // after test concluded, but not subsequent tests output.
  result.status = TestResult::TEST_FAILURE_ON_EXIT;
  EXPECT_EQ(GetTestOutputSnippet(result, output),
            "[ RUN      ] TestCase.FirstTest\n"
            "[       OK ] TestCase.FirstTest (0 ms)\n"
            "Post first test output\n");

  // test snippet of a failed test
  result.full_name = "TestCase.SecondTest";
  result.status = TestResult::TEST_FAILURE;
  EXPECT_EQ(GetTestOutputSnippet(result, output),
            "[ RUN      ] TestCase.SecondTest\n"
            "[  FAILED  ] TestCase.SecondTest (0 ms)\n");

  // test snippet of a skipped test. Note that the status is SUCCESS because
  // the gtest XML format doesn't make a difference between SUCCESS and SKIPPED
  result.full_name = "TestCase.ThirdTest";
  result.status = TestResult::TEST_SUCCESS;
  EXPECT_EQ(GetTestOutputSnippet(result, output),
            "[ RUN      ] TestCase.ThirdTest\n"
            "[  SKIPPED ] TestCase.ThirdTest (0 ms)\n");
}

void CheckTruncatationPreservesMessage(std::string message) {
  // Ensure the inserted message matches the expected pattern.
  constexpr char kExpected[] = R"(FATAL.*message\n)";
  ASSERT_THAT(message, ::testing::ContainsRegex(kExpected));

  const std::string snippet =
      base::StrCat({"[ RUN      ] SampleTestSuite.SampleTestName\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n",
                    message,
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"});

  // Strip the stack trace off the end of message.
  size_t line_end_pos = message.find("\n");
  std::string first_line = message.substr(0, line_end_pos + 1);

  const std::string result = TruncateSnippetFocused(snippet, 300);
  EXPECT_TRUE(result.find(first_line) > 0);
  EXPECT_EQ(result.length(), 300UL);
}

void MatchesFatalMessagesTest() {
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static base::NoDestructor<std::string> log_string;
  logging::SetLogMessageHandler([](int severity, const char* file, int line,
                                   size_t start,
                                   const std::string& str) -> bool {
    *log_string = str;
    return true;
  });
  // Different Chrome test suites have different settings for their logs.
  // E.g. unit tests may not show the process ID (as they are single process),
  // whereas browser tests usually do (as they are multi-process). This
  // affects how log messages are formatted and hence how the log criticality
  // i.e. "FATAL", appears in the log message. We test the two extremes --
  // all process IDs, timestamps present, and all not present. We also test
  // the presence/absence of an extra logging prefix.
  {
    // Process ID, Thread ID, Timestamp and Tickcount.
    logging::SetLogItems(true, true, true, true);
    logging::SetLogPrefix(nullptr);
    LOG(FATAL) << "message";
    CheckTruncatationPreservesMessage(*log_string);
  }
  {
    logging::SetLogItems(false, false, false, false);
    logging::SetLogPrefix(nullptr);
    LOG(FATAL) << "message";
    CheckTruncatationPreservesMessage(*log_string);
  }
  {
    // Process ID, Thread ID, Timestamp and Tickcount.
    logging::SetLogItems(true, true, true, true);
    logging::SetLogPrefix("my_log_prefix");
    LOG(FATAL) << "message";
    CheckTruncatationPreservesMessage(*log_string);
  }
  {
    logging::SetLogItems(false, false, false, false);
    logging::SetLogPrefix("my_log_prefix");
    LOG(FATAL) << "message";
    CheckTruncatationPreservesMessage(*log_string);
  }
}

// Validates TestSnippetFocused correctly identifies fatal messages to
// retain during truncation.
TEST(TestLauncherTools, TruncateSnippetFocusedMatchesFatalMessagesTest) {
  logging::ScopedLoggingSettings scoped_logging_settings;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_logging_settings.SetLogFormat(logging::LogFormat::LOG_FORMAT_SYSLOG);
#endif
  MatchesFatalMessagesTest();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Validates TestSnippetFocused correctly identifies fatal messages to
// retain during truncation, for ChromeOS Ash.
TEST(TestLauncherTools, TruncateSnippetFocusedMatchesFatalMessagesCrosAshTest) {
  logging::ScopedLoggingSettings scoped_logging_settings;
  scoped_logging_settings.SetLogFormat(logging::LogFormat::LOG_FORMAT_CHROME);
  MatchesFatalMessagesTest();
}
#endif

// Validate TestSnippetFocused truncates snippets correctly, regardless of
// whether fatal messages appear at the start, middle or end of the snippet.
TEST(TestLauncherTools, TruncateSnippetFocusedTest) {
  // Test where FATAL message appears in the start of the log.
  const std::string snippet =
      "[ RUN      ] "
      "EndToEndTests/"
      "EndToEndTest.WebTransportSessionUnidirectionalStreamSentEarly/"
      "draft29_QBIC\n"
      "[26219:26368:FATAL:tls_handshaker.cc(293)] 1-RTT secret(s) not set "
      "yet.\n"
      "#0 0x55619ad1fcdb in backtrace "
      "/b/s/w/ir/cache/builder/src/third_party/llvm/compiler-rt/lib/asan/../"
      "sanitizer_common/sanitizer_common_interceptors.inc:4205:13\n"
      "#1 0x5561a6bdf519 in base::debug::CollectStackTrace(void**, unsigned "
      "long) ./../../base/debug/stack_trace_posix.cc:845:39\n"
      "#2 0x5561a69a1293 in StackTrace "
      "./../../base/debug/stack_trace.cc:200:12\n"
      "...\n";
  const std::string result = TruncateSnippetFocused(snippet, 300);
  EXPECT_EQ(
      result,
      "[ RUN      ] EndToEndTests/EndToEndTest.WebTransportSessionUnidirection"
      "alStreamSentEarly/draft29_QBIC\n"
      "[26219:26368:FATAL:tls_handshaker.cc(293)] 1-RTT secret(s) not set "
      "yet.\n"
      "#0 0x55619ad1fcdb in backtrace /b/s/w/ir/cache/bui\n"
      "<truncated (358 bytes)>\n"
      "Trace ./../../base/debug/stack_trace.cc:200:12\n"
      "...\n");
  EXPECT_EQ(result.length(), 300UL);

  // Test where FATAL message appears in the middle of the log.
  const std::string snippet_two =
      "[ RUN      ] NetworkingPrivateApiTest.CreateSharedNetwork\n"
      "Padding log information added for testing purposes\n"
      "Padding log information added for testing purposes\n"
      "Padding log information added for testing purposes\n"
      "FATAL extensions_unittests[12666:12666]: [managed_network_configuration"
      "_handler_impl.cc(525)] Check failed: !guid_str && !guid_str->empty().\n"
      "#0 0x562f31dba779 base::debug::CollectStackTrace()\n"
      "#1 0x562f31cdf2a3 base::debug::StackTrace::StackTrace()\n"
      "#2 0x562f31cf4380 logging::LogMessage::~LogMessage()\n"
      "#3 0x562f31cf4d3e logging::LogMessage::~LogMessage()\n";
  const std::string result_two = TruncateSnippetFocused(snippet_two, 300);
  EXPECT_EQ(
      result_two,
      "[ RUN      ] NetworkingPriv\n"
      "<truncated (210 bytes)>\n"
      " added for testing purposes\n"
      "FATAL extensions_unittests[12666:12666]: [managed_network_configuration"
      "_handler_impl.cc(525)] Check failed: !guid_str && !guid_str->empty().\n"
      "#0 0x562f31dba779 base::deb\n"
      "<truncated (213 bytes)>\n"
      ":LogMessage::~LogMessage()\n");
  EXPECT_EQ(result_two.length(), 300UL);

  // Test where FATAL message appears at end of the log.
  const std::string snippet_three =
      "[ RUN      ] All/PDFExtensionAccessibilityTreeDumpTest.Highlights/"
      "linux\n"
      "[6741:6741:0716/171816.818448:ERROR:power_monitor_device_source_stub.cc"
      "(11)] Not implemented reached in virtual bool base::PowerMonitorDevice"
      "Source::IsOnBatteryPower()\n"
      "[6741:6741:0716/171816.818912:INFO:content_main_runner_impl.cc(1082)]"
      " Chrome is running in full browser mode.\n"
      "libva error: va_getDriverName() failed with unknown libva error,driver"
      "_name=(null)\n"
      "[6741:6741:0716/171817.688633:FATAL:agent_scheduling_group_host.cc(290)"
      "] Check failed: message->routing_id() != MSG_ROUTING_CONTROL "
      "(2147483647 vs. 2147483647)\n";
  const std::string result_three = TruncateSnippetFocused(snippet_three, 300);
  EXPECT_EQ(
      result_three,
      "[ RUN      ] All/PDFExtensionAccessibilityTreeDumpTest.Hi\n"
      "<truncated (432 bytes)>\n"
      "Name() failed with unknown libva error,driver_name=(null)\n"
      "[6741:6741:0716/171817.688633:FATAL:agent_scheduling_group_host.cc(290)"
      "] Check failed: message->routing_id() != MSG_ROUTING_CONTROL "
      "(2147483647 vs. 2147483647)\n");
  EXPECT_EQ(result_three.length(), 300UL);

  // Test where FATAL message does not appear.
  const std::string snippet_four =
      "[ RUN      ] All/PassingTest/linux\n"
      "Padding log line 1 added for testing purposes\n"
      "Padding log line 2 added for testing purposes\n"
      "Padding log line 3 added for testing purposes\n"
      "Padding log line 4 added for testing purposes\n"
      "Padding log line 5 added for testing purposes\n"
      "Padding log line 6 added for testing purposes\n";
  const std::string result_four = TruncateSnippetFocused(snippet_four, 300);
  EXPECT_EQ(result_four,
            "[ RUN      ] All/PassingTest/linux\n"
            "Padding log line 1 added for testing purposes\n"
            "Padding log line 2 added for testing purposes\n"
            "Padding lo\n<truncated (311 bytes)>\n"
            "Padding log line 4 added for testing purposes\n"
            "Padding log line 5 added for testing purposes\n"
            "Padding log line 6 added for testing purposes\n");
  EXPECT_EQ(result_four.length(), 300UL);
}

}  // namespace

}  // namespace base
