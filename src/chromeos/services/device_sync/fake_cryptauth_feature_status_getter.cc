// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_cryptauth_feature_status_getter.h"

namespace chromeos {

namespace device_sync {

FakeCryptAuthFeatureStatusGetter::FakeCryptAuthFeatureStatusGetter() = default;

FakeCryptAuthFeatureStatusGetter::~FakeCryptAuthFeatureStatusGetter() = default;

void FakeCryptAuthFeatureStatusGetter::FinishAttempt(
    const IdToFeatureStatusMap& id_to_feature_status_map,
    const CryptAuthDeviceSyncResult::ResultCode& device_sync_result_code) {
  DCHECK(request_context_);
  DCHECK(device_ids_);

  OnAttemptFinished(id_to_feature_status_map, device_sync_result_code);
}

void FakeCryptAuthFeatureStatusGetter::OnAttemptStarted(
    const cryptauthv2::RequestContext& request_context,
    const base::flat_set<std::string>& device_ids) {
  request_context_ = request_context;
  device_ids_ = device_ids;
}

FakeCryptAuthFeatureStatusGetterFactory::
    FakeCryptAuthFeatureStatusGetterFactory() = default;

FakeCryptAuthFeatureStatusGetterFactory::
    ~FakeCryptAuthFeatureStatusGetterFactory() = default;

std::unique_ptr<CryptAuthFeatureStatusGetter>
FakeCryptAuthFeatureStatusGetterFactory::BuildInstance(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  last_client_factory_ = client_factory;

  auto instance = std::make_unique<FakeCryptAuthFeatureStatusGetter>();
  instances_.push_back(instance.get());

  return instance;
}

}  // namespace device_sync

}  // namespace chromeos
