// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include <fuchsia/web/cpp/fidl.h>
#include <lib/zx/channel.h>

#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"
#include "fuchsia/engine/browser/web_engine_cdm_service.h"
#include "services/service_manager/public/cpp/binder_registry.h"

class WebEngineBrowserMainParts;

class WebEngineContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit WebEngineContentBrowserClient(
      fidl::InterfaceRequest<fuchsia::web::Context> request);
  ~WebEngineContentBrowserClient() override;

  WebEngineBrowserMainParts* main_parts_for_test() const { return main_parts_; }

  // ContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  std::string GetProduct() override;
  std::string GetUserAgent() override;
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* web_prefs) override;
  void BindInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  fidl::InterfaceRequest<fuchsia::web::Context> request_;

  // Owned by content::BrowserMainLoop.
  WebEngineBrowserMainParts* main_parts_;

  service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>
      mojo_service_registry_;
  WebEngineCdmService cdm_service_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineContentBrowserClient);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_
