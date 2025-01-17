// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/binary_fcm_service.h"

#include "base/run_loop.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

std::unique_ptr<KeyedService> BuildFakeGCMProfileService(
    content::BrowserContext* context) {
  return gcm::FakeGCMProfileService::Build(static_cast<Profile*>(context));
}

}  // namespace

class BinaryFCMServiceTest : public ::testing::Test {
 public:
  BinaryFCMServiceTest() {
    gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildFakeGCMProfileService));

    binary_fcm_service_ = BinaryFCMService::Create(&profile_);
  }

  Profile* profile() { return &profile_; }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<BinaryFCMService> binary_fcm_service_;
};

TEST_F(BinaryFCMServiceTest, GetsInstanceID) {
  std::string received_instance_id = BinaryFCMService::kInvalidId;

  // Allow |binary_fcm_service_| to get an instance id.
  content::RunAllTasksUntilIdle();

  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &received_instance_id));

  content::RunAllTasksUntilIdle();

  EXPECT_NE(received_instance_id, BinaryFCMService::kInvalidId);
}

TEST_F(BinaryFCMServiceTest, RoutesMessages) {
  DeepScanningClientResponse response1;
  DeepScanningClientResponse response2;

  binary_fcm_service_->SetCallbackForToken(
      "token1", base::BindRepeating(
                    [](DeepScanningClientResponse* target_response,
                       DeepScanningClientResponse response) {
                      *target_response = response;
                    },
                    &response1));
  binary_fcm_service_->SetCallbackForToken(
      "token2", base::BindRepeating(
                    [](DeepScanningClientResponse* target_response,
                       DeepScanningClientResponse response) {
                      *target_response = response;
                    },
                    &response2));

  DeepScanningClientResponse message;
  std::string serialized_message;
  gcm::IncomingMessage incoming_message;

  // Test that a message with token1 is routed only to the first callback.
  message.set_token("token1");
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  incoming_message.data["proto"] = serialized_message;
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response1.token(), "token1");
  EXPECT_EQ(response2.token(), "");

  // Test that a message with token2 is routed only to the second callback.
  message.set_token("token2");
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  incoming_message.data["proto"] = serialized_message;
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response1.token(), "token1");
  EXPECT_EQ(response2.token(), "token2");

  // Test that I can clear a callback
  response2.clear_token();
  binary_fcm_service_->ClearCallbackForToken("token2");
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response2.token(), "");
}

}  // namespace safe_browsing
