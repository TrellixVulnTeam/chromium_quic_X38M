// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/webauthn_commands.h"

#include <utility>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/session.h"

namespace {

static constexpr char kBase64UrlError[] = " must be a base64url encoded string";

// Creates a base::DictionaryValue by cloning the parameters specified by
// |mapping| from |params|.
base::DictionaryValue MapParams(
    const base::flat_map<const char*, const char*>& mapping,
    const base::Value& params) {
  base::DictionaryValue options;
  for (const std::pair<const char*, const char*>& pair : mapping) {
    const base::Value* value = params.FindKey(pair.second);
    if (value)
      options.SetPath(pair.first, value->Clone());
  }
  return options;
}

// Converts the string |keys| in |params| from base64url to base64. Returns a
// status error if conversion of one of the keys failed.
Status ConvertBase64UrlToBase64(base::Value* params,
                                const std::vector<std::string> keys) {
  for (const std::string key : keys) {
    base::Value* maybe_value = params->FindKey(key);
    if (!maybe_value)
      continue;

    if (!maybe_value->is_string())
      return Status(kInvalidArgument, key + kBase64UrlError);

    std::string& value = maybe_value->GetString();
    std::string temp;
    if (!Base64UrlDecode(value, base::Base64UrlDecodePolicy::IGNORE_PADDING,
                         &temp)) {
      return Status(kInvalidArgument, key + kBase64UrlError);
    }

    base::Base64Encode(temp, &value);
  }

  return Status(kOk);
}

// Converts the string |keys| in |params| from base64 to base64url.
void ConvertBase64ToBase64Url(base::Value* params,
                              const std::vector<std::string> keys) {
  for (const std::string key : keys) {
    std::string* maybe_value = params->FindStringKey(key);
    if (!maybe_value)
      continue;

    std::string temp;
    bool result = base::Base64Decode(*maybe_value, &temp);
    DCHECK(result);

    base::Base64UrlEncode(temp, base::Base64UrlEncodePolicy::OMIT_PADDING,
                          maybe_value);
  }
}

}  // namespace

Status ExecuteWebAuthnCommand(const WebAuthnCommand& command,
                              Session* session,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;

  status = web_view->SendCommand("WebAuthn.enable", base::DictionaryValue());
  if (status.IsError())
    return status;

  return command.Run(web_view, params, value);
}

Status ExecuteAddVirtualAuthenticator(WebView* web_view,
                                      const base::Value& params,
                                      std::unique_ptr<base::Value>* value) {
  return web_view->SendCommandAndGetResult(
      "WebAuthn.addVirtualAuthenticator",
      MapParams(
          {
              {"options.protocol", "protocol"},
              {"options.transport", "transport"},
              {"options.hasResidentKey", "hasResidentKey"},
              {"options.hasUserVerification", "hasUserVerification"},
              {"options.automaticPresenceSimulation", "isUserVerified"},
          },
          params),
      value);
}

Status ExecuteRemoveVirtualAuthenticator(WebView* web_view,
                                         const base::Value& params,
                                         std::unique_ptr<base::Value>* value) {
  return web_view->SendCommandAndGetResult(
      "WebAuthn.removeVirtualAuthenticator",
      MapParams({{"authenticatorId", "authenticatorId"}}, params), value);
}

Status ExecuteAddCredential(WebView* web_view,
                            const base::Value& params,
                            std::unique_ptr<base::Value>* value) {
  base::DictionaryValue mapped_params = MapParams(
      {
          {"authenticatorId", "authenticatorId"},
          {"credential.credentialId", "credentialId"},
          {"credential.isResidentCredential", "isResidentCredential"},
          {"credential.rpId", "rpId"},
          {"credential.privateKey", "privateKey"},
          {"credential.userHandle", "userHandle"},
          {"credential.signCount", "signCount"},
      },
      params);
  Status status =
      ConvertBase64UrlToBase64(mapped_params.FindKey("credential"),
                               {"credentialId", "privateKey", "userHandle"});
  if (status.IsError())
    return status;

  return web_view->SendCommandAndGetResult("WebAuthn.addCredential",
                                           std::move(mapped_params), value);
}

Status ExecuteGetCredentials(WebView* web_view,
                             const base::Value& params,
                             std::unique_ptr<base::Value>* value) {
  Status status = web_view->SendCommandAndGetResult(
      "WebAuthn.getCredentials",
      MapParams({{"authenticatorId", "authenticatorId"}}, params), value);
  if (status.IsError())
    return status;

  for (base::Value& credential : (*value)->FindKey("credentials")->GetList()) {
    ConvertBase64ToBase64Url(&credential,
                             {"credentialId", "privateKey", "userHandle"});
  }

  return status;
}
