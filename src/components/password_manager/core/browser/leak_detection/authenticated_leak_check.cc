// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {
namespace {

using ::signin::AccessTokenFetcher;

constexpr char kAPIScope[] = "https://www.googleapis.com/auth/userinfo.id";

// Returns a Google account that can be used for getting a token.
CoreAccountId GetAccountForRequest(
    const signin::IdentityManager* identity_manager) {
  CoreAccountInfo result = identity_manager->GetUnconsentedPrimaryAccountInfo();
  if (result.IsEmpty()) {
    std::vector<CoreAccountInfo> all_accounts =
        identity_manager->GetAccountsWithRefreshTokens();
    if (!all_accounts.empty())
      result = all_accounts.front();
  }
  return result.account_id;
}

}  // namespace

// Incapsulates the token request and payload calculation done in parallel.
class AuthenticatedLeakCheck::RequestPayloadHelper {
 public:
  RequestPayloadHelper(
      AuthenticatedLeakCheck* leak_check,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~RequestPayloadHelper() = default;

  // Neither copyable nor movable.
  RequestPayloadHelper(const RequestPayloadHelper&) = delete;
  RequestPayloadHelper& operator=(const RequestPayloadHelper&) = delete;
  RequestPayloadHelper(RequestPayloadHelper&&) = delete;
  RequestPayloadHelper& operator=(RequestPayloadHelper&&) = delete;

  void RequestAccessToken(AccessTokenFetcher::TokenCallback callback);
  void PreparePayload(const std::string& username,
                      const std::string& password,
                      SingleLeakRequestDataCallback callback);

  // Notifies that the access token was obtained.
  void OnGotAccessToken(std::string access_token);

  // Notifies that the payload was obtained.
  void OnGotPayload(LookupSingleLeakData data);

 private:
  // Describes the steps done in parallel as a bit mask.
  enum Step {
    kPayloadData = 1 << 0,
    kAccessToken = 1 << 1,
    kAll = kPayloadData | kAccessToken
  };

  // If both the access token and the payload are ready notify |leak_check_|.
  void CheckAllStepsDone();

  // Bitmask of steps done.
  int steps_ = 0;
  // Owns |this|.
  AuthenticatedLeakCheck* leak_check_;
  // Identity manager for the profile.
  signin::IdentityManager* identity_manager_;
  // URL loader factory required for the network request to the identity
  // endpoint.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Actual request for the needed token.
  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;
  // The token to be used for request.
  std::string access_token_;
  // Payload for the actual request.
  LookupSingleLeakData payload_;
};

AuthenticatedLeakCheck::RequestPayloadHelper::RequestPayloadHelper(
    AuthenticatedLeakCheck* leak_check,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : leak_check_(leak_check),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(identity_manager_);
  DCHECK(url_loader_factory_);
}

void AuthenticatedLeakCheck::RequestPayloadHelper::RequestAccessToken(
    AccessTokenFetcher::TokenCallback callback) {
  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      GetAccountForRequest(identity_manager_),
      /*consumer_name=*/"leak_detection_service", {kAPIScope},
      std::move(callback), signin::AccessTokenFetcher::Mode::kImmediate);
}

void AuthenticatedLeakCheck::RequestPayloadHelper::PreparePayload(
    const std::string& username,
    const std::string& password,
    SingleLeakRequestDataCallback callback) {
  PrepareSingleLeakRequestData(username, password, std::move(callback));
}

void AuthenticatedLeakCheck::RequestPayloadHelper::OnGotAccessToken(
    std::string access_token) {
  access_token_ = std::move(access_token);
  steps_ |= kAccessToken;
  token_fetcher_.reset();

  CheckAllStepsDone();
}

void AuthenticatedLeakCheck::RequestPayloadHelper::OnGotPayload(
    LookupSingleLeakData data) {
  payload_ = std::move(data);
  steps_ |= kPayloadData;

  CheckAllStepsDone();
}

void AuthenticatedLeakCheck::RequestPayloadHelper::CheckAllStepsDone() {
  if (steps_ == kAll) {
    leak_check_->DoLeakRequest(std::move(payload_), std::move(access_token_),
                               std::move(url_loader_factory_));
  }
}

AuthenticatedLeakCheck::AuthenticatedLeakCheck(
    LeakDetectionDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : delegate_(delegate),
      payload_helper_(new RequestPayloadHelper(this,
                                               identity_manager,
                                               std::move(url_loader_factory))),
      network_request_factory_(std::make_unique<LeakDetectionRequestFactory>()),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
}

AuthenticatedLeakCheck::~AuthenticatedLeakCheck() = default;

// static
bool AuthenticatedLeakCheck::HasAccountForRequest(
    const signin::IdentityManager* identity_manager) {
  // On desktop HasUnconsentedPrimaryAccount() will always return something if
  // the user is signed in.
  // On Android it will be empty if the user isn't syncing. Thus,
  // GetAccountsWithRefreshTokens() check is necessary.
  return identity_manager &&
         (identity_manager->HasUnconsentedPrimaryAccount() ||
          !identity_manager->GetAccountsWithRefreshTokens().empty());
}

void AuthenticatedLeakCheck::Start(const GURL& url,
                                   base::StringPiece16 username,
                                   base::StringPiece16 password) {
  DCHECK(payload_helper_);
  DCHECK(!request_);

  url_ = url;
  username_ = base::UTF16ToUTF8(username);
  payload_helper_->RequestAccessToken(
      base::BindOnce(&AuthenticatedLeakCheck::OnAccessTokenRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  payload_helper_->PreparePayload(
      username_, base::UTF16ToUTF8(password),
      base::BindOnce(&AuthenticatedLeakCheck::OnRequestDataReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuthenticatedLeakCheck::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(ERROR) << "Token request error: " << error.error_message();
    delegate_->OnError(LeakDetectionError::kTokenRequestFailure);
    return;
  }

  // The fetcher successfully obtained an access token.
  DVLOG(0) << "Token=" << access_token_info.token;
  payload_helper_->OnGotAccessToken(std::move(access_token_info.token));
}

void AuthenticatedLeakCheck::OnRequestDataReady(LookupSingleLeakData data) {
  if (data.encryption_key.empty()) {
    DLOG(ERROR) << "Preparing the payload for leak  detection failed";
    delegate_->OnError(LeakDetectionError::kHashingFailure);
    return;
  }
  payload_helper_->OnGotPayload(std::move(data));
}

void AuthenticatedLeakCheck::DoLeakRequest(
    LookupSingleLeakData data,
    std::string access_token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  payload_helper_.reset();
  encryption_key_ = std::move(data.encryption_key);
  request_ = network_request_factory_->CreateNetworkRequest();
  request_->LookupSingleLeak(
      url_loader_factory.get(), access_token,
      std::move(data.username_hash_prefix), std::move(data.encrypted_payload),
      base::BindOnce(&AuthenticatedLeakCheck::OnLookupSingleLeakResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuthenticatedLeakCheck::OnLookupSingleLeakResponse(
    std::unique_ptr<SingleLookupResponse> response) {
  if (!response) {
    delegate_->OnError(LeakDetectionError::kInvalidServerResponse);
    return;
  }

  delegate_->OnLeakDetectionDone(ParseLookupSingleLeakResponse(*response), url_,
                                 base::UTF8ToUTF16(username_));
}

}  // namespace password_manager
