// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_ANDROID_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_ANDROID_H_

#include "base/macros.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

class SharingService;

// Handles incoming messages for the shared clipboard feature.
class SharedClipboardMessageHandler : public SharingMessageHandler {
 public:
  explicit SharedClipboardMessageHandler(SharingService* sharing_service);
  ~SharedClipboardMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(
      const chrome_browser_sharing::SharingMessage& message) override;

 private:
  SharingService* sharing_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_ANDROID_H_
