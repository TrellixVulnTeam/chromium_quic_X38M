// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "crypto/sha2.h"

namespace password_manager {
namespace {

// Despite the function is short, it executes long. That's why it should be done
// asynchronously.
LookupSingleLeakData PrepareLookupSingleLeakData(const std::string& username,
                                                 const std::string& password) {
  LookupSingleLeakData data;
  data.username_hash_prefix = BucketizeUsername(CanonicalizeUsername(username));
  data.encrypted_payload = CipherEncrypt(
      ScryptHashUsernameAndPassword(username, password), &data.encryption_key);
  return data;
}

// Searches |reencrypted_lookup_hash| in the |encrypted_leak_match_prefixes|
// array. |encryption_key| is the original client key used to encrypt the
// payload.
bool CheckIfCredentialWasLeaked(std::unique_ptr<SingleLookupResponse> response,
                                const std::string& encryption_key) {
  std::string decrypted_username_password =
      CipherDecrypt(response->reencrypted_lookup_hash, encryption_key);
  if (decrypted_username_password.empty()) {
    DLOG(ERROR) << "Can't decrypt data="
                << base::HexEncode(base::as_bytes(
                       base::make_span(response->reencrypted_lookup_hash)));
    return false;
  }

  std::string hash_username_password =
      crypto::SHA256HashString(decrypted_username_password);

  return std::any_of(response->encrypted_leak_match_prefixes.begin(),
                     response->encrypted_leak_match_prefixes.end(),
                     [&hash_username_password](const std::string& prefix) {
                       return base::StartsWith(hash_username_password, prefix,
                                               base::CompareCase::SENSITIVE);
                     });
}

}  // namespace

void PrepareSingleLeakRequestData(const std::string& username,
                                  const std::string& password,
                                  SingleLeakRequestDataCallback callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&PrepareLookupSingleLeakData, username, password),
      std::move(callback));
}

void AnalyzeResponseResult(std::unique_ptr<SingleLookupResponse> response,
                           const std::string& encryption_key,
                           SingleLeakResponseAnalysisCallback callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CheckIfCredentialWasLeaked, std::move(response),
                     encryption_key),
      std::move(callback));
}

bool ParseLookupSingleLeakResponse(const SingleLookupResponse& response) {
  // TODO(crbug.com/086298): Implement decrypting the response and checking
  // whether the credential was actually leaked.
  DVLOG(0) << "Number of Encrypted Leak Match Prefixes: "
           << response.encrypted_leak_match_prefixes.size();

  base::span<const char> hash = response.reencrypted_lookup_hash;
  DVLOG(0) << "Reencrypted Lookup Hash: "
           << base::HexEncode(base::as_bytes(hash));
  return false;
}

}  // namespace password_manager
