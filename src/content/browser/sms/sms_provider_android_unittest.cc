// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/sms/sms_provider.h"
#include "content/browser/sms/sms_provider_android.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/content_unittests_jni_headers/Fakes_jni.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;
using ::testing::NiceMock;
using url::Origin;

namespace content {

namespace {

class MockObserver : public SmsProvider::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD2(OnReceive, bool(const Origin&, const std::string& sms));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

// SmsProviderAndroidTest tests the JNI bindings to the android SmsReceiver
// and the handling of the SMS upon retrieval.
class SmsProviderAndroidTest : public RenderViewHostTestHarness {
 protected:
  SmsProviderAndroidTest() {}
  ~SmsProviderAndroidTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    j_fake_sms_retriever_client_.Reset(
        Java_FakeSmsRetrieverClient_create(AttachCurrentThread()));
    Java_Fakes_setClientForTesting(AttachCurrentThread(),
                                   provider_.GetSmsReceiverForTesting(),
                                   j_fake_sms_retriever_client_);
    provider_.AddObserver(&observer_);
  }

  void TriggerSms(const std::string& sms) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FakeSmsRetrieverClient_triggerSms(
        env, j_fake_sms_retriever_client_,
        base::android::ConvertUTF8ToJavaString(env, sms));
  }

  SmsProviderAndroid& provider() { return provider_; }

  NiceMock<MockObserver>* observer() { return &observer_; }

 private:
  SmsProviderAndroid provider_;
  NiceMock<MockObserver> observer_;
  base::android::ScopedJavaGlobalRef<jobject> j_fake_sms_retriever_client_;

  DISALLOW_COPY_AND_ASSIGN(SmsProviderAndroidTest);
};

}  // namespace

TEST_F(SmsProviderAndroidTest, Retrieve) {
  std::string test_url = "https://www.google.com";
  std::string expected_sms = "Hi \nFor: " + test_url;

  EXPECT_CALL(*observer(),
              OnReceive(Origin::Create(GURL(test_url)), expected_sms));
  provider().Retrieve();
  TriggerSms(expected_sms);
}

TEST_F(SmsProviderAndroidTest, IgnoreBadSms) {
  std::string test_url = "https://www.google.com";
  std::string good_sms = "Hi \nFor: " + test_url;
  std::string bad_sms = "Hi \nFor: http://b.com";

  EXPECT_CALL(*observer(), OnReceive(Origin::Create(GURL(test_url)), good_sms));

  provider().Retrieve();
  TriggerSms(bad_sms);
  TriggerSms(good_sms);
}

}  // namespace content
