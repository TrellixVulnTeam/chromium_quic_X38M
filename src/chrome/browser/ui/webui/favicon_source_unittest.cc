// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include <memory>
#include <utility>

#include "base/test/bind_test_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/resources/grit/ui_resources.h"

using GotDataCallback = content::URLDataSource::GotDataCallback;
using WebContentsGetter = content::WebContents::Getter;
using testing::_;
using testing::Return;
using testing::ReturnArg;

namespace {

const int kDummyTaskId = 1;

}  // namespace

void Noop(scoped_refptr<base::RefCountedMemory>) {}

class MockHistoryUiFaviconRequestHandler
    : public favicon::HistoryUiFaviconRequestHandler {
 public:
  MockHistoryUiFaviconRequestHandler() = default;
  ~MockHistoryUiFaviconRequestHandler() override = default;

  MOCK_METHOD7(
      GetRawFaviconForPageURL,
      void(const GURL& page_url,
           int desired_size_in_pixel,
           favicon_base::FaviconRawBitmapCallback callback,
           favicon::FaviconRequestPlatform request_platform,
           favicon::HistoryUiFaviconRequestOrigin request_origin_for_uma,
           const GURL& icon_url_for_uma,
           base::CancelableTaskTracker* tracker));

  MOCK_METHOD5(
      GetFaviconImageForPageURL,
      void(const GURL& page_url,
           favicon_base::FaviconImageCallback callback,
           favicon::HistoryUiFaviconRequestOrigin request_origin_for_uma,
           const GURL& icon_url_for_uma,
           base::CancelableTaskTracker* tracker));
};

class TestFaviconSource : public FaviconSource {
 public:
  TestFaviconSource(chrome::FaviconUrlFormat format,
                    Profile* profile,
                    ui::NativeTheme* theme)
      : FaviconSource(profile, format), theme_(theme) {}

  ~TestFaviconSource() override {}

  MOCK_METHOD2(LoadIconBytes, base::RefCountedMemory*(float, int));

 protected:
  ui::NativeTheme* GetNativeTheme() override { return theme_; }

 private:
  ui::NativeTheme* const theme_;
};

class FaviconSourceTestBase : public testing::Test {
 public:
  explicit FaviconSourceTestBase(chrome::FaviconUrlFormat format)
      : source_(format, &profile_, &theme_) {
    // Setup testing factories for main dependencies.
    BrowserContextKeyedServiceFactory::TestingFactory
        history_ui_favicon_request_handler_factory =
            base::BindRepeating([](content::BrowserContext*) {
              return base::WrapUnique<KeyedService>(
                  new MockHistoryUiFaviconRequestHandler());
            });
    mock_history_ui_favicon_request_handler_ =
        static_cast<MockHistoryUiFaviconRequestHandler*>(
            HistoryUiFaviconRequestHandlerFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    &profile_, history_ui_favicon_request_handler_factory));
    BrowserContextKeyedServiceFactory::TestingFactory favicon_service_factory =
        base::BindRepeating([](content::BrowserContext*) {
          return static_cast<std::unique_ptr<KeyedService>>(
              std::make_unique<favicon::MockFaviconService>());
        });
    mock_favicon_service_ = static_cast<favicon::MockFaviconService*>(
        FaviconServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, favicon_service_factory));

    // Setup TestWebContents.
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    test_web_contents_getter_ = base::BindLambdaForTesting(
        [&] { return (content::WebContents*)test_web_contents_.get(); });

    // On call, dependencies will return empty favicon by default.
    ON_CALL(*mock_favicon_service_, GetRawFaviconForPageURL(_, _, _, _, _, _))
        .WillByDefault([](auto, auto, auto, auto,
                          favicon_base::FaviconRawBitmapCallback callback,
                          auto) {
          std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
          return kDummyTaskId;
        });
    ON_CALL(*mock_history_ui_favicon_request_handler_,
            GetRawFaviconForPageURL(_, _, _, _, _, _, _))
        .WillByDefault([](auto, auto,
                          favicon_base::FaviconRawBitmapCallback callback, auto,
                          auto, auto, auto) {
          std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        });

    // Mock default icon loading.
    ON_CALL(*source(), LoadIconBytes(_, _))
        .WillByDefault(Return(kDummyIconBytes.get()));
  }

  void SetDarkMode(bool dark_mode) { theme_.SetDarkMode(dark_mode); }

  TestFaviconSource* source() { return &source_; }

 protected:
  const scoped_refptr<base::RefCountedBytes> kDummyIconBytes;
  content::TestBrowserThreadBundle thread_bundle_;
  ui::TestNativeTheme theme_;
  TestingProfile profile_;
  MockHistoryUiFaviconRequestHandler* mock_history_ui_favicon_request_handler_;
  favicon::MockFaviconService* mock_favicon_service_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  WebContentsGetter test_web_contents_getter_;
  TestFaviconSource source_;
};

class FaviconSourceTestWithLegacyFormat : public FaviconSourceTestBase {
 public:
  FaviconSourceTestWithLegacyFormat()
      : FaviconSourceTestBase(chrome::FaviconUrlFormat::kFaviconLegacy) {}
};

class FaviconSourceTestWithFavicon2Format : public FaviconSourceTestBase {
 public:
  FaviconSourceTestWithFavicon2Format()
      : FaviconSourceTestBase(chrome::FaviconUrlFormat::kFavicon2) {}
};

TEST_F(FaviconSourceTestWithLegacyFormat, DarkDefault) {
  SetDarkMode(true);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON_DARK));
  source()->StartDataRequest(std::string(), test_web_contents_getter_,
                             base::BindRepeating(&Noop));
}

TEST_F(FaviconSourceTestWithLegacyFormat, LightDefault) {
  SetDarkMode(false);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source()->StartDataRequest(std::string(), test_web_contents_getter_,
                             base::BindRepeating(&Noop));
}

TEST_F(FaviconSourceTestWithLegacyFormat,
       ShouldNotQueryHistoryUiFaviconRequestHandler) {
  content::WebContentsTester::For(test_web_contents_.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIHistoryURL));

  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);

  source()->StartDataRequest("size/16@1x/https://www.google.com",
                             test_web_contents_getter_,
                             base::BindRepeating(&Noop));
}

TEST_F(FaviconSourceTestWithFavicon2Format, DarkDefault) {
  SetDarkMode(true);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON_DARK));
  source()->StartDataRequest(std::string(), test_web_contents_getter_,
                             base::BindRepeating(&Noop));
}

TEST_F(FaviconSourceTestWithFavicon2Format, LightDefault) {
  SetDarkMode(false);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source()->StartDataRequest(std::string(), test_web_contents_getter_,
                             base::BindRepeating(&Noop));
}

TEST_F(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryHistoryUiFaviconRequestHandlerIfNotAllowed) {
  content::WebContentsTester::For(test_web_contents_.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIHistoryURL));

  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);

  source()->StartDataRequest(
      "?size=16&scale_factor=1x&page_url=https%3A%2F%2Fwww.google."
      "com&allow_google_server_fallback=0",
      test_web_contents_getter_, base::BindRepeating(&Noop));
}

TEST_F(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryHistoryUiFaviconRequestHandlerIfHasNotHistoryUiOrigin) {
  content::WebContentsTester::For(test_web_contents_.get())
      ->SetLastCommittedURL(GURL("chrome://non-history-url"));

  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);

  source()->StartDataRequest(
      "?size=16&scale_factor=1x&page_url=https%3A%2F%2Fwww.google."
      "com&allow_google_server_fallback=1",
      test_web_contents_getter_, base::BindRepeating(&Noop));
}

TEST_F(
    FaviconSourceTestWithFavicon2Format,
    ShouldQueryHistoryUiFaviconRequestHandlerIfHasHistoryUiOriginAndAllowed) {
  content::WebContentsTester::For(test_web_contents_.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIHistoryURL));

  EXPECT_CALL(
      *mock_history_ui_favicon_request_handler_,
      GetRawFaviconForPageURL(GURL("https://www.google.com"), _, _, _, _, _, _))
      .Times(1);

  source()->StartDataRequest(
      "?size=16&scale_factor=1x&page_url=https%3A%2F%2Fwww.google."
      "com&allow_google_server_fallback=1",
      test_web_contents_getter_, base::BindRepeating(&Noop));
}
