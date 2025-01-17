// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_c.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "components/cronet/native/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ExecutorsTest : public ::testing::Test {
 public:
  ExecutorsTest() = default;
  ~ExecutorsTest() override = default;

 protected:
  static void TestRunnable_Run(Cronet_RunnablePtr self);
  bool runnable_called() const { return runnable_called_; }

  // Provide a task environment for use by TestExecutor instances. Do not
  // initialize the ThreadPool as this is done by the Cronet_Engine
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::ThreadingMode::MAIN_THREAD_ONLY};

 private:
  void set_runnable_called(bool value) { runnable_called_ = value; }

  bool runnable_called_ = false;
  DISALLOW_COPY_AND_ASSIGN(ExecutorsTest);
};

// App implementation of Cronet_Executor methods.
void TestExecutor_Execute(Cronet_ExecutorPtr self, Cronet_RunnablePtr command) {
  CHECK(self);
  Cronet_Runnable_Run(command);
  Cronet_Runnable_Destroy(command);
}

// Implementation of TestRunnable methods.
// static
void ExecutorsTest::TestRunnable_Run(Cronet_RunnablePtr self) {
  CHECK(self);
  Cronet_ClientContext context = Cronet_Runnable_GetClientContext(self);
  ExecutorsTest* test = static_cast<ExecutorsTest*>(context);
  CHECK(test);
  test->set_runnable_called(true);
}

// Test that custom Executor defined by the app runs the runnable.
TEST_F(ExecutorsTest, TestCustom) {
  ASSERT_FALSE(runnable_called());
  Cronet_RunnablePtr runnable =
      Cronet_Runnable_CreateWith(ExecutorsTest::TestRunnable_Run);
  Cronet_Runnable_SetClientContext(runnable, this);
  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  Cronet_Executor_Execute(executor, runnable);
  Cronet_Executor_Destroy(executor);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(runnable_called());
}

// Test that cronet::test::TestExecutor runs the runnable.
TEST_F(ExecutorsTest, TestTestExecutor) {
  ASSERT_FALSE(runnable_called());
  Cronet_RunnablePtr runnable = Cronet_Runnable_CreateWith(TestRunnable_Run);
  Cronet_Runnable_SetClientContext(runnable, this);
  Cronet_ExecutorPtr executor = cronet::test::CreateTestExecutor();
  Cronet_Executor_Execute(executor, runnable);
  Cronet_Executor_Destroy(executor);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(runnable_called());
}

}  // namespace
