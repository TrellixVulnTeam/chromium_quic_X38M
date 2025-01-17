// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_H_
#define CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/services/cups_proxy/public/mojom/proxy.mojom.h"

namespace cups_proxy {

class CupsProxyServiceDelegate;

// This service lives in the browser process and is managed by the
// CupsProxyServiceManager. It bootstraps/maintains a mojom connection with the
// CupsProxyDaemon.
//
// Note: There is no method granting a service handle since beyond creation,
// this service's only client is the daemon, who's connection is managed
// internally.
class CupsProxyService {
 public:
  // Spawns the global service instance.
  static void Spawn(std::unique_ptr<CupsProxyServiceDelegate> delegate);

 private:
  friend base::NoDestructor<CupsProxyService>;
  CupsProxyService();
  ~CupsProxyService();

  // Records whether we've attempted connection with the daemon yet.
  bool bootstrap_attempted_ = false;

  // Methods for connecting with the CupsProxyDaemon.
  void BindToCupsProxyDaemon(
      std::unique_ptr<CupsProxyServiceDelegate> delegate);
  void OnBindToCupsProxyDaemon(bool success);

  base::WeakPtrFactory<CupsProxyService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CupsProxyService);
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_H_
