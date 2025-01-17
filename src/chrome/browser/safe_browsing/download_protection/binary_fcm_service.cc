// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/binary_fcm_service.h"

#include <memory>

#include "base/logging.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/safe_browsing/proto/webprotect.pb.h"

namespace safe_browsing {

namespace {

const char kBinaryFCMServiceAppId[] = "safe_browsing_fcm_service";
// TODO(drubery): Once the server side has finalized their sender id, fill this
// in.
const char kBinaryFCMServiceSenderId[] = "SenderID";
const char kBinaryFCMServiceMessageKey[] = "proto";

}  // namespace

const char BinaryFCMService::kInvalidId[] = "";

// static
std::unique_ptr<BinaryFCMService> BinaryFCMService::Create(Profile* profile) {
  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);
  if (!gcm_profile_service)
    return nullptr;

  gcm::GCMDriver* gcm_driver = gcm_profile_service->driver();
  if (!gcm_driver)
    return nullptr;

  instance_id::InstanceIDProfileService* instance_id_profile_service =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile);
  if (!instance_id_profile_service)
    return nullptr;

  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id_profile_service->driver();
  if (!instance_id_driver)
    return nullptr;

  return std::make_unique<BinaryFCMService>(gcm_driver, instance_id_driver);
}

BinaryFCMService::BinaryFCMService(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver)
    : gcm_driver_(gcm_driver),
      instance_id_(kInvalidId),
      weakptr_factory_(this) {
  gcm_driver->AddAppHandler(kBinaryFCMServiceAppId, this);
  instance_id_driver->GetInstanceID(kBinaryFCMServiceAppId)
      ->GetToken(kBinaryFCMServiceSenderId, instance_id::kGCMScope,
                 /*options=*/{},
                 /*flags=*/{},
                 base::BindOnce(&BinaryFCMService::OnGetInstanceID,
                                weakptr_factory_.GetWeakPtr()));
}

BinaryFCMService::~BinaryFCMService() {
  gcm_driver_->RemoveAppHandler(kBinaryFCMServiceAppId);
}

void BinaryFCMService::GetInstanceID(GetInstanceIDCallback callback) {
  std::move(callback).Run(instance_id_);
}

void BinaryFCMService::SetCallbackForToken(
    const std::string& token,
    base::RepeatingCallback<void(DeepScanningClientResponse)> callback) {
  message_token_map_[token] = std::move(callback);
}

void BinaryFCMService::ClearCallbackForToken(const std::string& token) {
  message_token_map_.erase(token);
}

void BinaryFCMService::OnGetInstanceID(const std::string& instance_id,
                                       instance_id::InstanceID::Result result) {
  if (result == instance_id::InstanceID::Result::SUCCESS) {
    instance_id_ = instance_id;
  }
}

void BinaryFCMService::ShutdownHandler() {
  NOTIMPLEMENTED();
}

void BinaryFCMService::OnStoreReset() {
  NOTIMPLEMENTED();
}

void BinaryFCMService::OnMessage(const std::string& app_id,
                                 const gcm::IncomingMessage& message) {
  auto serialized_proto = message.data.find(kBinaryFCMServiceMessageKey);
  if (serialized_proto == message.data.end())
    return;

  DeepScanningClientResponse response;
  if (!response.ParseFromString(serialized_proto->second))
    return;

  auto callback_it = message_token_map_.find(response.token());
  if (callback_it == message_token_map_.end())
    return;

  callback_it->second.Run(std::move(response));
}

void BinaryFCMService::OnMessagesDeleted(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void BinaryFCMService::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  NOTIMPLEMENTED();
}

void BinaryFCMService::OnSendAcknowledged(const std::string& app_id,
                                          const std::string& message_id) {
  NOTIMPLEMENTED();
}

bool BinaryFCMService::CanHandle(const std::string& app_id) const {
  return app_id == kBinaryFCMServiceAppId;
}

}  // namespace safe_browsing
