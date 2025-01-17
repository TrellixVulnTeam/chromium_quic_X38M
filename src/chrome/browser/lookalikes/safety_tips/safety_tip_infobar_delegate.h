// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_INFOBAR_DELEGATE_H_

#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace safety_tips {

class SafetyTipInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SafetyTipInfoBarDelegate(SafetyTipType type,
                           const GURL& url,
                           content::WebContents* web_contents);

  // ConfirmInfoBarDelegate:
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // infobars::InfoBarDelegate
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  void InfoBarDismissed() override;

 private:
  SafetyTipType type_;
  GURL url_;
  content::WebContents* web_contents_;
};

}  // namespace safety_tips

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_INFOBAR_DELEGATE_H_
