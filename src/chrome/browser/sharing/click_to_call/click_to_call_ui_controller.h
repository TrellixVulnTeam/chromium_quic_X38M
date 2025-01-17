// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_ui_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class ClickToCallUiController
    : public SharingUiController,
      public content::WebContentsUserData<ClickToCallUiController> {
 public:
  static ClickToCallUiController* GetOrCreateFromWebContents(
      content::WebContents* web_contents);
  static void ShowDialog(content::WebContents* web_contents,
                         const GURL& url,
                         bool hide_default_handler);

  ~ClickToCallUiController() override;

  void OnDeviceSelected(const GURL& url, const syncer::DeviceInfo& device);

  // Overridden from SharingUiController:
  base::string16 GetTitle() override;
  PageActionIconType GetIconType() override;
  int GetRequiredDeviceCapabilities() override;
  void OnDeviceChosen(const syncer::DeviceInfo& device) override;
  void OnAppChosen(const App& app) override;

  // Called by the ClickToCallDialogView when the help text got clicked.
  void OnHelpTextClicked();

 protected:
  explicit ClickToCallUiController(content::WebContents* web_contents);

  // Overridden from SharingUiController:
  SharingDialog* DoShowDialog(BrowserWindow* window) override;
  void DoUpdateApps(UpdateAppsCallback callback) override;

 private:
  friend class content::WebContentsUserData<ClickToCallUiController>;


  GURL phone_url_;
  bool hide_default_handler_ = false;

  base::WeakPtrFactory<ClickToCallUiController> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ClickToCallUiController);
};

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UI_CONTROLLER_H_
