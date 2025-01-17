// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_H_

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "components/download/public/common/download_url_parameters.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace download {
struct DownloadCreateInfo;
class DownloadURLLoaderFactoryGetter;
class InputStream;

// Class for handling the download of a url. Implemented by child classes.
class COMPONENTS_DOWNLOAD_EXPORT UrlDownloadHandler {
 public:
  using UniqueUrlDownloadHandlerPtr =
      std::unique_ptr<UrlDownloadHandler, base::OnTaskRunnerDeleter>;
  // Class to be notified when download starts/stops.
  class COMPONENTS_DOWNLOAD_EXPORT Delegate {
   public:
    virtual void OnUrlDownloadStarted(
        std::unique_ptr<DownloadCreateInfo> download_create_info,
        std::unique_ptr<InputStream> input_stream,
        scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
        UrlDownloadHandler* downloader,
        const DownloadUrlParameters::OnStartedCallback& callback) = 0;

    // Called after the connection is cancelled or finished.
    virtual void OnUrlDownloadStopped(UrlDownloadHandler* downloader) = 0;

    // Called when a UrlDownloadHandler is created.
    virtual void OnUrlDownloadHandlerCreated(
        UniqueUrlDownloadHandlerPtr downloader) {}
  };

  UrlDownloadHandler() = default;
  virtual ~UrlDownloadHandler() = default;

  // Called on the io thread to pause the url request.
  virtual void PauseRequest() {}

  // Called on the io thread to resume the url request.
  virtual void ResumeRequest() {}

  // Called on the io thread to cancel the url request.
  virtual void CancelRequest() {}

  DISALLOW_COPY_AND_ASSIGN(UrlDownloadHandler);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_H_
