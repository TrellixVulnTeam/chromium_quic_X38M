// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_H_

#include <memory>
#include <string>

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace password_manager {

// This class encapsulates the logic required to talk to the identity leak check
// endpoint. Callers are expected to construct an instance for each request they
// would like to perform. Destruction of the class results in a cancellation of
// the initiated network request.
class LeakDetectionRequest : public LeakDetectionRequestInterface {
 public:
  // TODO(crbug.com/986298): Switch to production endpoint once available.
  static constexpr char kLookupSingleLeakEndpoint[] =
      "https://autopush-passwordsleakcheck-pa.sandbox.googleapis.com/v1/"
      "leaks:lookupSingle";

  LeakDetectionRequest();
  ~LeakDetectionRequest() override;

  // Initiates a leak lookup network request for the credential corresponding to
  // |username_hash_prefix| and |encrypted_payload|. |access_token| is required
  // to authenticate the request. Invokes |callback| on completion, unless this
  // instance is deleted beforehand. If the request failed, |callback| is
  // invoked with |nullptr|, otherwise a SingleLookupResponse is returned.
  void LookupSingleLeak(network::mojom::URLLoaderFactory* url_loader_factory,
                        const std::string& access_token,
                        std::string username_hash_prefix,
                        std::string encrypted_payload,
                        LookupSingleLeakCallback callback) override;

 private:
  void OnLookupSingleLeakResponse(LookupSingleLeakCallback callback,
                                  std::unique_ptr<std::string> response);

  // Simple URL loader required for the network request to the identity
  // endpoint.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_H_
