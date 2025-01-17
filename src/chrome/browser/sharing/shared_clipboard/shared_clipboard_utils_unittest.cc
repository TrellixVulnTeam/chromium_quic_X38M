// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_context_menu_observer.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_utils.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

const char kEmptyText[] = "";
const char kText[] = "Some text to copy to phone device.";

class MockSharingService : public SharingService {
 public:
  explicit MockSharingService(std::unique_ptr<SharingFCMHandler> fcm_handler)
      : SharingService(/* sync_prefs= */ nullptr,
                       /* vapid_key_manager= */ nullptr,
                       /* sharing_device_registration= */ nullptr,
                       /* fcm_sender= */ nullptr,
                       std::move(fcm_handler),
                       /* gcm_driver= */ nullptr,
                       /* device_info_tracker= */ nullptr,
                       /* local_device_info_provider= */ nullptr,
                       /* sync_service */ nullptr) {}

  ~MockSharingService() override = default;

  MOCK_CONST_METHOD0(GetState, State());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSharingService);
};

class SharedClipboardUtilsTest : public testing::Test {
 public:
  SharedClipboardUtilsTest() = default;

  ~SharedClipboardUtilsTest() override = default;

  void SetUp() override {
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&SharedClipboardUtilsTest::CreateService,
                                       base::Unretained(this)));
  }

 protected:
  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    if (!create_service_)
      return nullptr;

    return std::make_unique<NiceMock<MockSharingService>>(
        std::make_unique<SharingFCMHandler>(nullptr, nullptr));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  bool create_service_ = true;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardUtilsTest);
};

}  // namespace

TEST_F(SharedClipboardUtilsTest, UIFlagDisabled_DoNotShowMenu) {
  scoped_feature_list_.InitAndDisableFeature(kSharedClipboardUI);
  EXPECT_FALSE(
      ShouldOfferSharedClipboard(&profile_, base::ASCIIToUTF16(kText)));
}

TEST_F(SharedClipboardUtilsTest, IncognitoProfile_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  EXPECT_FALSE(ShouldOfferSharedClipboard(profile_.GetOffTheRecordProfile(),
                                          base::ASCIIToUTF16(kText)));
}

TEST_F(SharedClipboardUtilsTest, EmptyClipboardProtocol_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  EXPECT_FALSE(
      ShouldOfferSharedClipboard(&profile_, base::ASCIIToUTF16(kEmptyText)));
}

TEST_F(SharedClipboardUtilsTest, ClipboardProtocol_ShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  EXPECT_TRUE(ShouldOfferSharedClipboard(&profile_, base::ASCIIToUTF16(kText)));
}

TEST_F(SharedClipboardUtilsTest, NoSharingService_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  create_service_ = false;
  EXPECT_FALSE(
      ShouldOfferSharedClipboard(&profile_, base::ASCIIToUTF16(kText)));
}
