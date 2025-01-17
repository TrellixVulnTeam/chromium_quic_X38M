// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "ui/base/clipboard/clipboard.h"

class GURL;

namespace ui {
class ScopedClipboardWriter;
}  // namespace ui

namespace content {

class ClipboardHostImplTest;

class CONTENT_EXPORT ClipboardHostImpl : public blink::mojom::ClipboardHost {
 public:
  ~ClipboardHostImpl() override;

  // TODO(https://crbug.com/955171): Remove this and use Create directly once
  // RenderProcessHostImpl uses service_manager::BinderMap instead of
  // service_manager::BinderRegistry.
  static void CreateForRequest(blink::mojom::ClipboardHostRequest request);

  static void Create(
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);

 private:
  friend class ClipboardHostImplTest;

  explicit ClipboardHostImpl(
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);

  // content::mojom::ClipboardHost
  void GetSequenceNumber(ui::ClipboardType clipboard_type,
                         GetSequenceNumberCallback callback) override;
  void IsFormatAvailable(blink::mojom::ClipboardFormat format,
                         ui::ClipboardType clipboard_type,
                         IsFormatAvailableCallback callback) override;
  void ReadAvailableTypes(ui::ClipboardType clipboard_type,
                          ReadAvailableTypesCallback callback) override;
  void ReadText(ui::ClipboardType clipboard_type,
                ReadTextCallback callback) override;
  void ReadHtml(ui::ClipboardType clipboard_type,
                ReadHtmlCallback callback) override;
  void ReadRtf(ui::ClipboardType clipboard_type,
               ReadRtfCallback callback) override;
  void ReadImage(ui::ClipboardType clipboard_type,
                 ReadImageCallback callback) override;
  void ReadCustomData(ui::ClipboardType clipboard_type,
                      const base::string16& type,
                      ReadCustomDataCallback callback) override;
  void WriteText(const base::string16& text) override;
  void WriteHtml(const base::string16& markup, const GURL& url) override;
  void WriteSmartPasteMarker() override;
  void WriteCustomData(
      const base::flat_map<base::string16, base::string16>& data) override;
  void WriteBookmark(const std::string& url,
                     const base::string16& title) override;
  void WriteImage(const SkBitmap& bitmap) override;
  void CommitWrite() override;
#if defined(OS_MACOSX)
  void WriteStringToFindPboard(const base::string16& text) override;
#endif

  mojo::Receiver<blink::mojom::ClipboardHost> receiver_;
  ui::Clipboard* const clipboard_;  // Not owned
  std::unique_ptr<ui::ScopedClipboardWriter> clipboard_writer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
