// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/pending_app_manager_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/test/bind_test_util.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/pending_app_install_task.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using InstallAppsResults = std::vector<std::pair<GURL, InstallResultCode>>;
using UninstallAppsResults = std::vector<std::pair<GURL, bool>>;

const GURL kFooWebAppUrl("https://foo.example");
const GURL kBarWebAppUrl("https://bar.example");
const GURL kQuxWebAppUrl("https://qux.example");

ExternalInstallOptions GetFooInstallOptions(
    base::Optional<bool> override_previous_user_uninstall =
        base::Optional<bool>()) {
  ExternalInstallOptions options(kFooWebAppUrl, LaunchContainer::kTab,
                                 ExternalInstallSource::kExternalPolicy);

  if (override_previous_user_uninstall.has_value())
    options.override_previous_user_uninstall =
        *override_previous_user_uninstall;

  return options;
}

ExternalInstallOptions GetBarInstallOptions() {
  ExternalInstallOptions options(kBarWebAppUrl, LaunchContainer::kWindow,
                                 ExternalInstallSource::kExternalPolicy);
  return options;
}

ExternalInstallOptions GetQuxInstallOptions() {
  ExternalInstallOptions options(kQuxWebAppUrl, LaunchContainer::kWindow,
                                 ExternalInstallSource::kExternalPolicy);
  return options;
}

std::string GenerateFakeAppId(const GURL& url) {
  return TestInstallFinalizer::GetAppIdForUrl(url);
}

class TestPendingAppManagerImpl : public PendingAppManagerImpl {
 public:
  TestPendingAppManagerImpl(Profile* profile,
                            TestAppRegistrar* test_app_registrar)
      : PendingAppManagerImpl(profile),
        test_app_registrar_(test_app_registrar) {}

  ~TestPendingAppManagerImpl() override {
    DCHECK(next_installation_task_results_.empty());
  }

  size_t install_run_count() { return install_run_count_; }

  const std::vector<ExternalInstallOptions>& install_options_list() {
    return install_options_list_;
  }

  void SetNextInstallationTaskResult(const GURL& app_url,
                                     InstallResultCode result_code) {
    DCHECK(!base::Contains(next_installation_task_results_, app_url));
    next_installation_task_results_[app_url] = result_code;
  }

  std::unique_ptr<PendingAppInstallTask> CreateInstallationTask(
      ExternalInstallOptions install_options) override {
    return std::make_unique<TestPendingAppInstallTask>(
        this, profile(), std::move(install_options));
  }

  void OnInstallCalled(const ExternalInstallOptions& install_options) {
    ++install_run_count_;
    install_options_list_.push_back(install_options);
  }

  InstallResultCode GetNextInstallationTaskResult(const GURL& url) {
    DCHECK(base::Contains(next_installation_task_results_, url));
    auto result = next_installation_task_results_.at(url);
    next_installation_task_results_.erase(url);
    return result;
  }

  TestAppRegistrar* registrar() { return test_app_registrar_; }

 private:
  class TestPendingAppInstallTask : public PendingAppInstallTask {
   public:
    TestPendingAppInstallTask(
        TestPendingAppManagerImpl* pending_app_manager_impl,
        Profile* profile,
        ExternalInstallOptions install_options)
        : PendingAppInstallTask(profile,
                                pending_app_manager_impl->registrar(),
                                pending_app_manager_impl->finalizer(),
                                install_options),
          pending_app_manager_impl_(pending_app_manager_impl),
          externally_installed_app_prefs_(profile->GetPrefs()) {}
    ~TestPendingAppInstallTask() override = default;

    void Install(content::WebContents* web_contents,
                 WebAppUrlLoader::Result url_loaded_result,
                 ResultCallback callback) override {
      pending_app_manager_impl_->OnInstallCalled(install_options());

      base::Optional<AppId> app_id;
      const GURL& install_url = install_options().url;
      auto result_code =
          pending_app_manager_impl_->GetNextInstallationTaskResult(install_url);
      if (result_code == InstallResultCode::kSuccess) {
        app_id = GenerateFakeAppId(install_url);
        pending_app_manager_impl_->registrar()->AddExternalApp(
            *app_id, {install_url, install_options().install_source});
        externally_installed_app_prefs_.Insert(
            install_url, *app_id, install_options().install_source);
        const bool is_placeholder =
            (url_loaded_result != WebAppUrlLoader::Result::kUrlLoaded);
        externally_installed_app_prefs_.SetIsPlaceholder(install_url,
                                                         is_placeholder);
      }
      std::move(callback).Run({result_code, app_id});
    }

   private:
    TestPendingAppManagerImpl* pending_app_manager_impl_;
    ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

    DISALLOW_COPY_AND_ASSIGN(TestPendingAppInstallTask);
  };

  TestAppRegistrar* test_app_registrar_;

  std::vector<ExternalInstallOptions> install_options_list_;
  size_t install_run_count_ = 0;

  std::map<GURL, InstallResultCode> next_installation_task_results_;
};

}  // namespace

class PendingAppManagerImplTest : public ChromeRenderViewHostTestHarness {
 public:
  PendingAppManagerImplTest() = default;

  ~PendingAppManagerImplTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    auto* provider = web_app::TestWebAppProvider::Get(profile());

    auto test_app_registrar = std::make_unique<TestAppRegistrar>();
    app_registrar_ = test_app_registrar.get();
    provider->SetRegistrar(std::move(test_app_registrar));

    auto test_pending_app_manager =
        std::make_unique<TestPendingAppManagerImpl>(profile(), app_registrar_);
    pending_app_manager_impl_ = test_pending_app_manager.get();
    provider->SetPendingAppManager(std::move(test_pending_app_manager));

    auto url_loader = std::make_unique<TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();
    pending_app_manager_impl_->SetUrlLoaderForTesting(std::move(url_loader));

    auto test_install_finalizer = std::make_unique<TestInstallFinalizer>();
    install_finalizer_ = test_install_finalizer.get();
    provider->SetInstallFinalizer(std::move(test_install_finalizer));

    auto ui_manager = std::make_unique<TestWebAppUiManager>();
    ui_manager_ = ui_manager.get();
    provider->SetWebAppUiManager(std::move(ui_manager));

    provider->Start();
  }

 protected:
  std::pair<GURL, InstallResultCode> InstallAndWait(
      PendingAppManager* pending_app_manager,
      ExternalInstallOptions install_options) {
    base::RunLoop run_loop;

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;

    pending_app_manager_impl()->Install(
        std::move(install_options),
        base::BindLambdaForTesting([&](const GURL& u, InstallResultCode c) {
          url = u;
          code = c;
          run_loop.Quit();
        }));
    run_loop.Run();

    return {url.value(), code.value()};
  }

  std::vector<std::pair<GURL, InstallResultCode>> InstallAppsAndWait(
      PendingAppManager* pending_app_manager,
      std::vector<ExternalInstallOptions> apps_to_install) {
    std::vector<std::pair<GURL, InstallResultCode>> results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_install.size(), run_loop.QuitClosure());
    pending_app_manager_impl()->InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url, InstallResultCode code) {
              results.emplace_back(url, code);
              barrier_closure.Run();
            }));
    run_loop.Run();

    return results;
  }

  std::vector<std::pair<GURL, bool>> UninstallAppsAndWait(
      PendingAppManager* pending_app_manager,
      std::vector<GURL> apps_to_uninstall) {
    std::vector<std::pair<GURL, bool>> results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_uninstall.size(), run_loop.QuitClosure());
    pending_app_manager->UninstallApps(
        std::move(apps_to_uninstall),
        base::BindLambdaForTesting(
            [&](const GURL& url, bool successfully_uninstalled) {
              results.emplace_back(url, successfully_uninstalled);
              barrier_closure.Run();
            }));
    run_loop.Run();

    return results;
  }

  // ExternalInstallOptions that was used to run the last installation task.
  const ExternalInstallOptions& last_install_options() {
    DCHECK(!pending_app_manager_impl_->install_options_list().empty());
    return pending_app_manager_impl_->install_options_list().back();
  }

  // Number of times PendingAppInstallTask::Install was called. Reflects
  // how many times we've tried to create a web app.
  size_t install_run_count() {
    return pending_app_manager_impl_->install_run_count();
  }

  size_t uninstall_call_count() {
    return install_finalizer_->uninstall_external_web_app_urls().size();
  }

  const std::vector<GURL>& uninstalled_app_urls() {
    return install_finalizer_->uninstall_external_web_app_urls();
  }

  const GURL& last_uninstalled_app_url() {
    return install_finalizer_->uninstall_external_web_app_urls().back();
  }

  TestPendingAppManagerImpl* pending_app_manager_impl() {
    return pending_app_manager_impl_;
  }

  TestAppRegistrar* registrar() { return app_registrar_; }

  TestWebAppUiManager* ui_manager() { return ui_manager_; }

  TestWebAppUrlLoader* url_loader() { return url_loader_; }

  TestInstallFinalizer* install_finalizer() { return install_finalizer_; }

 private:
  TestAppRegistrar* app_registrar_ = nullptr;
  TestPendingAppManagerImpl* pending_app_manager_impl_ = nullptr;
  TestInstallFinalizer* install_finalizer_ = nullptr;
  TestWebAppUiManager* ui_manager_ = nullptr;
  TestWebAppUrlLoader* url_loader_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PendingAppManagerImplTest);
};

TEST_F(PendingAppManagerImplTest, Install_Succeeds) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  base::Optional<GURL> url;
  base::Optional<InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

  EXPECT_EQ(InstallResultCode::kSuccess, code.value());
  EXPECT_EQ(kFooWebAppUrl, url.value());

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetFooInstallOptions(), last_install_options());
}

TEST_F(PendingAppManagerImplTest, Install_SerialCallsDifferentApps) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetFooInstallOptions(), last_install_options());
  }

  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kBarWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;

    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetBarInstallOptions());

    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(kBarWebAppUrl, url.value());

    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(GetBarInstallOptions(), last_install_options());
  }
}

TEST_F(PendingAppManagerImplTest, Install_ConcurrentCallsDifferentApps) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kBarWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kFooWebAppUrl, url);

        // Two installations tasks should have run at this point,
        // one from the last call to install (which gets higher priority),
        // and another one for this call to install.
        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetFooInstallOptions(), last_install_options());

        run_loop.Quit();
      }));
  pending_app_manager_impl()->Install(
      GetBarInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kBarWebAppUrl, url);

        // The last call gets higher priority so only one
        // installation task should have run at this point.
        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetBarInstallOptions(), last_install_options());
      }));
  run_loop.Run();
}

TEST_F(PendingAppManagerImplTest, Install_PendingSuccessfulTask) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kBarWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SaveLoadUrlRequests();

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kFooWebAppUrl, url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptions(), last_install_options());

        foo_run_loop.Quit();
      }));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  pending_app_manager_impl()->Install(
      GetBarInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kBarWebAppUrl, url);

        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetBarInstallOptions(), last_install_options());

        bar_run_loop.Quit();
      }));

  url_loader()->ProcessLoadUrlRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  url_loader()->ProcessLoadUrlRequests();
  bar_run_loop.Run();
}

TEST_F(PendingAppManagerImplTest, Install_PendingFailingTask) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kFailedUnknownReason);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kBarWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SaveLoadUrlRequests();

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kFailedUnknownReason, code);
        EXPECT_EQ(kFooWebAppUrl, url);

        EXPECT_EQ(1u, install_run_count());

        foo_run_loop.Quit();
      }));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  pending_app_manager_impl()->Install(
      GetBarInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kBarWebAppUrl, url);

        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetBarInstallOptions(), last_install_options());

        bar_run_loop.Quit();
      }));

  url_loader()->ProcessLoadUrlRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  url_loader()->ProcessLoadUrlRequests();
  bar_run_loop.Run();
}

TEST_F(PendingAppManagerImplTest, Install_ReentrantCallback) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kBarWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  auto final_callback =
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kBarWebAppUrl, url);

        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetBarInstallOptions(), last_install_options());
        run_loop.Quit();
      });
  auto reentrant_callback =
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kFooWebAppUrl, url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptions(), last_install_options());

        pending_app_manager_impl()->Install(GetBarInstallOptions(),
                                            final_callback);
      });

  // Call Install() with a callback that tries to install another app.
  pending_app_manager_impl()->Install(GetFooInstallOptions(),
                                      reentrant_callback);
  run_loop.Run();
}

TEST_F(PendingAppManagerImplTest, Install_SerialCallsSameApp) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(InstallResultCode::kSuccess, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetFooInstallOptions(), last_install_options());
  }

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(InstallResultCode::kAlreadyInstalled, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    // The app is already installed so we shouldn't try to install it again.
    EXPECT_EQ(1u, install_run_count());
  }
}

TEST_F(PendingAppManagerImplTest, Install_ConcurrentCallsSameApp) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  bool first_callback_ran = false;

  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        // kAlreadyInstalled because the last call to Install gets higher
        // priority.
        EXPECT_EQ(InstallResultCode::kAlreadyInstalled, code);
        EXPECT_EQ(kFooWebAppUrl, url);

        // Only one installation task should run because the app was already
        // installed.
        EXPECT_EQ(1u, install_run_count());

        EXPECT_TRUE(first_callback_ran);

        run_loop.Quit();
      }));

  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kFooWebAppUrl, url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptions(), last_install_options());
        first_callback_ran = true;
      }));
  run_loop.Run();

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetFooInstallOptions(), last_install_options());
}

TEST_F(PendingAppManagerImplTest, Install_AlwaysUpdate) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  auto get_force_reinstall_info = []() {
    ExternalInstallOptions options(kFooWebAppUrl, LaunchContainer::kWindow,
                                   ExternalInstallSource::kExternalPolicy);
    options.force_reinstall = true;
    return options;
  };

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), get_force_reinstall_info());

    EXPECT_EQ(InstallResultCode::kSuccess, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(get_force_reinstall_info(), last_install_options());
  }

  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), get_force_reinstall_info());

    EXPECT_EQ(InstallResultCode::kSuccess, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    // The app should be installed again because of the |force_reinstall| flag.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(get_force_reinstall_info(), last_install_options());
  }
}

TEST_F(PendingAppManagerImplTest, Install_InstallationFails) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kFailedUnknownReason);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::Optional<GURL> url;
  base::Optional<InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

  EXPECT_EQ(InstallResultCode::kFailedUnknownReason, code);
  EXPECT_EQ(kFooWebAppUrl, url);

  EXPECT_EQ(1u, install_run_count());
}

TEST_F(PendingAppManagerImplTest, Install_PlaceholderApp) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(
      kFooWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  base::Optional<GURL> url;
  base::Optional<InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(pending_app_manager_impl(), install_options);

  EXPECT_EQ(InstallResultCode::kSuccess, code);
  EXPECT_EQ(kFooWebAppUrl, url);

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(install_options, last_install_options());
}

TEST_F(PendingAppManagerImplTest, InstallApps_Succeeds) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());

  InstallAppsResults results = InstallAppsAndWait(pending_app_manager_impl(),
                                                  std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults({{kFooWebAppUrl, InstallResultCode::kSuccess}}));

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetFooInstallOptions(), last_install_options());
}

TEST_F(PendingAppManagerImplTest, InstallApps_FailsInstallationFails) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kFailedUnknownReason);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());

  InstallAppsResults results = InstallAppsAndWait(pending_app_manager_impl(),
                                                  std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{kFooWebAppUrl, InstallResultCode::kFailedUnknownReason}}));

  EXPECT_EQ(1u, install_run_count());
}

TEST_F(PendingAppManagerImplTest, InstallApps_PlaceholderApp) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(
      kFooWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;
  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(install_options);

  InstallAppsResults results = InstallAppsAndWait(pending_app_manager_impl(),
                                                  std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults({{kFooWebAppUrl, InstallResultCode::kSuccess}}));

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(install_options, last_install_options());
}

TEST_F(PendingAppManagerImplTest, InstallApps_Multiple) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kBarWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.push_back(GetBarInstallOptions());

  InstallAppsResults results = InstallAppsAndWait(pending_app_manager_impl(),
                                                  std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults({{kFooWebAppUrl, InstallResultCode::kSuccess},
                                {kBarWebAppUrl, InstallResultCode::kSuccess}}));

  EXPECT_EQ(2u, install_run_count());
  EXPECT_EQ(GetBarInstallOptions(), last_install_options());
}

TEST_F(PendingAppManagerImplTest, InstallApps_PendingInstallApps) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kBarWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  {
    std::vector<ExternalInstallOptions> apps_to_install;
    apps_to_install.push_back(GetFooInstallOptions());

    pending_app_manager_impl()->InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url, InstallResultCode code) {
              EXPECT_EQ(InstallResultCode::kSuccess, code);
              EXPECT_EQ(kFooWebAppUrl, url);

              EXPECT_EQ(1u, install_run_count());
              EXPECT_EQ(GetFooInstallOptions(), last_install_options());
            }));
  }

  {
    std::vector<ExternalInstallOptions> apps_to_install;
    apps_to_install.push_back(GetBarInstallOptions());

    pending_app_manager_impl()->InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url, InstallResultCode code) {
              EXPECT_EQ(InstallResultCode::kSuccess, code);
              EXPECT_EQ(kBarWebAppUrl, url);

              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetBarInstallOptions(), last_install_options());

              run_loop.Quit();
            }));
  }
  run_loop.Run();
}

TEST_F(PendingAppManagerImplTest, Install_PendingMulitpleInstallApps) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kBarWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kQuxWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kQuxWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.push_back(GetBarInstallOptions());

  // Queue through InstallApps.
  int callback_calls = 0;
  pending_app_manager_impl()->InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        ++callback_calls;
        if (callback_calls == 1) {
          EXPECT_EQ(InstallResultCode::kSuccess, code);
          EXPECT_EQ(kFooWebAppUrl, url);

          EXPECT_EQ(2u, install_run_count());
          EXPECT_EQ(GetFooInstallOptions(), last_install_options());
        } else if (callback_calls == 2) {
          EXPECT_EQ(InstallResultCode::kSuccess, code);
          EXPECT_EQ(kBarWebAppUrl, url);

          EXPECT_EQ(3u, install_run_count());
          EXPECT_EQ(GetBarInstallOptions(), last_install_options());

          run_loop.Quit();
        } else {
          NOTREACHED();
        }
      }));

  // Queue through Install.
  pending_app_manager_impl()->Install(
      GetQuxInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kQuxWebAppUrl, url);

        // The install request from Install should be processed first.
        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetQuxInstallOptions(), last_install_options());
      }));

  run_loop.Run();
}

TEST_F(PendingAppManagerImplTest, InstallApps_PendingInstall) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kBarWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kBarWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kQuxWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kQuxWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  // Queue through Install.
  pending_app_manager_impl()->Install(
      GetQuxInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kQuxWebAppUrl, url);

        // The install request from Install should be processed first.
        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetQuxInstallOptions(), last_install_options());
      }));

  // Queue through InstallApps.
  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.push_back(GetBarInstallOptions());

  int callback_calls = 0;
  pending_app_manager_impl()->InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        ++callback_calls;
        if (callback_calls == 1) {
          EXPECT_EQ(InstallResultCode::kSuccess, code);
          EXPECT_EQ(kFooWebAppUrl, url);

          // The install requests from InstallApps should be processed next.
          EXPECT_EQ(2u, install_run_count());
          EXPECT_EQ(GetFooInstallOptions(), last_install_options());

          return;
        }
        if (callback_calls == 2) {
          EXPECT_EQ(InstallResultCode::kSuccess, code);
          EXPECT_EQ(kBarWebAppUrl, url);

          EXPECT_EQ(3u, install_run_count());
          EXPECT_EQ(GetBarInstallOptions(), last_install_options());

          run_loop.Quit();
          return;
        }
        NOTREACHED();
      }));
  run_loop.Run();
}

TEST_F(PendingAppManagerImplTest, AppUninstalled) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
  }

  // Simulate the app getting uninstalled.
  registrar()->RemoveExternalAppByInstallUrl(kFooWebAppUrl);

  // Try to install the app again.
  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                       WebAppUrlLoader::Result::kUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    // The app was uninstalled so a new installation task should run.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
  }
}

TEST_F(PendingAppManagerImplTest, ExternalAppUninstalled) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
  }

  // Simulate external app for the app getting uninstalled by the user.
  const std::string app_id = GenerateFakeAppId(kFooWebAppUrl);
  registrar()->SimulateExternalAppUninstalledByUser(app_id);

  // The app was uninstalled by the user. Installing again should succeed
  // or fail depending on whether we set override_previous_user_uninstall. We
  // try with override_previous_user_uninstall false first, true second.
  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(
        pending_app_manager_impl(),
        GetFooInstallOptions(false /* override_previous_user_uninstall */));

    // The app shouldn't be installed because the user previously uninstalled
    // it, so there shouldn't be any new installation task runs.
    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(InstallResultCode::kPreviouslyUninstalled, code.value());
  }

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                       WebAppUrlLoader::Result::kUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(
        pending_app_manager_impl(),
        GetFooInstallOptions(true /* override_previous_user_uninstall */));

    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
  }
}

TEST_F(PendingAppManagerImplTest, UninstallApps_Succeeds) {
  registrar()->AddExternalApp(
      GenerateFakeAppId(kFooWebAppUrl),
      {kFooWebAppUrl, ExternalInstallSource::kExternalPolicy});

  install_finalizer()->SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                            true);
  UninstallAppsResults results = UninstallAppsAndWait(
      pending_app_manager_impl(), std::vector<GURL>{kFooWebAppUrl});

  EXPECT_EQ(results, UninstallAppsResults({{kFooWebAppUrl, true}}));

  EXPECT_EQ(1u, uninstall_call_count());
  EXPECT_EQ(kFooWebAppUrl, last_uninstalled_app_url());
}

TEST_F(PendingAppManagerImplTest, UninstallApps_Fails) {
  install_finalizer()->SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                            false);
  UninstallAppsResults results = UninstallAppsAndWait(
      pending_app_manager_impl(), std::vector<GURL>{kFooWebAppUrl});
  EXPECT_EQ(results, UninstallAppsResults({{kFooWebAppUrl, false}}));

  EXPECT_EQ(1u, uninstall_call_count());
  EXPECT_EQ(kFooWebAppUrl, last_uninstalled_app_url());
}

TEST_F(PendingAppManagerImplTest, UninstallApps_Multiple) {
  registrar()->AddExternalApp(
      GenerateFakeAppId(kFooWebAppUrl),
      {kFooWebAppUrl, ExternalInstallSource::kExternalPolicy});
  registrar()->AddExternalApp(
      GenerateFakeAppId(kBarWebAppUrl),
      {kFooWebAppUrl, ExternalInstallSource::kExternalPolicy});

  install_finalizer()->SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                            true);
  install_finalizer()->SetNextUninstallExternalWebAppResult(kBarWebAppUrl,
                                                            true);
  UninstallAppsResults results =
      UninstallAppsAndWait(pending_app_manager_impl(),
                           std::vector<GURL>{kFooWebAppUrl, kBarWebAppUrl});
  EXPECT_EQ(results, UninstallAppsResults(
                         {{kFooWebAppUrl, true}, {kBarWebAppUrl, true}}));

  EXPECT_EQ(2u, uninstall_call_count());
  EXPECT_EQ(std::vector<GURL>({kFooWebAppUrl, kBarWebAppUrl}),
            uninstalled_app_urls());
}

TEST_F(PendingAppManagerImplTest, UninstallApps_PendingInstall) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      kFooWebAppUrl, InstallResultCode::kSuccess);
  url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccess, code);
        EXPECT_EQ(kFooWebAppUrl, url);
        run_loop.Quit();
      }));

  install_finalizer()->SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                            false);
  UninstallAppsResults uninstall_results = UninstallAppsAndWait(
      pending_app_manager_impl(), std::vector<GURL>{kFooWebAppUrl});
  EXPECT_EQ(uninstall_results, UninstallAppsResults({{kFooWebAppUrl, false}}));
  EXPECT_EQ(1u, uninstall_call_count());

  run_loop.Run();
}

TEST_F(PendingAppManagerImplTest, ReinstallPlaceholderApp_Success) {
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    url_loader()->SetNextLoadUrlResult(
        kFooWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                       WebAppUrlLoader::Result::kUrlLoaded);
    install_finalizer()->SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                              true);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(PendingAppManagerImplTest,
       ReinstallPlaceholderApp_ReinstallNotPossible) {
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    url_loader()->SetNextLoadUrlResult(
        kFooWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Try to reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    url_loader()->SetNextLoadUrlResult(
        kFooWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    // Even though the placeholder app is already install, we make a call to
    // InstallFinalizer. InstallFinalizer ensures we don't unnecessarily
    // install the placeholder app again.
    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(PendingAppManagerImplTest,
       ReinstallPlaceholderAppWhenUnused_NoOpenedWindows) {
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    url_loader()->SetNextLoadUrlResult(
        kFooWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    install_options.wait_for_windows_closed = true;
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    ui_manager()->SetNumWindowsForApp(GenerateFakeAppId(kFooWebAppUrl), 0);
    url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                       WebAppUrlLoader::Result::kUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(PendingAppManagerImplTest,
       ReinstallPlaceholderAppWhenUnused_OneWindowOpened) {
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    url_loader()->SetNextLoadUrlResult(
        kFooWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    install_options.wait_for_windows_closed = true;
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        kFooWebAppUrl, InstallResultCode::kSuccess);
    ui_manager()->SetNumWindowsForApp(GenerateFakeAppId(kFooWebAppUrl), 1);
    url_loader()->SetNextLoadUrlResult(kFooWebAppUrl,
                                       WebAppUrlLoader::Result::kUrlLoaded);
    install_finalizer()->SetNextUninstallExternalWebAppResult(kFooWebAppUrl,
                                                              true);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccess, code.value());
    EXPECT_EQ(kFooWebAppUrl, url.value());

    EXPECT_EQ(2u, install_run_count());
  }
}

}  // namespace web_app
