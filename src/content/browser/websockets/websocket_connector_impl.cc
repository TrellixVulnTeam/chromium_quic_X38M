// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/websockets/websocket_connector_impl.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace content {

WebSocketConnectorImpl::WebSocketConnectorImpl(int process_id,
                                               int frame_id,
                                               const url::Origin& origin)
    : process_id_(process_id), frame_id_(frame_id), origin_(origin) {}

WebSocketConnectorImpl::~WebSocketConnectorImpl() = default;

void WebSocketConnectorImpl::Connect(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    const base::Optional<std::string>& user_agent,
    network::mojom::WebSocketHandshakeClientPtr handshake_client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  if (!process) {
    return;
  }
  RenderFrameHost* frame = RenderFrameHost::FromID(process_id_, frame_id_);
  const uint32_t options =
      GetContentClient()->browser()->GetWebSocketOptions(frame);

  if (GetContentClient()->browser()->WillInterceptWebSocket(frame)) {
    GetContentClient()->browser()->CreateWebSocket(
        frame,
        base::BindOnce(ConnectCalledByContentBrowserClient, requested_protocols,
                       site_for_cookies, process_id_, frame_id_, origin_,
                       options),
        url, site_for_cookies, user_agent, std::move(handshake_client));
    return;
  }
  std::vector<network::mojom::HttpHeaderPtr> headers;
  if (user_agent) {
    headers.push_back(network::mojom::HttpHeader::New(
        net::HttpRequestHeaders::kUserAgent, *user_agent));
  }
  process->GetStoragePartition()->GetNetworkContext()->CreateWebSocket(
      url, requested_protocols, site_for_cookies, std::move(headers),
      process_id_, frame_id_, origin_, options, std::move(handshake_client),
      nullptr, nullptr);
}

void WebSocketConnectorImpl::ConnectCalledByContentBrowserClient(
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    int process_id,
    int frame_id,
    const url::Origin& origin,
    uint32_t options,
    const GURL& url,
    std::vector<network::mojom::HttpHeaderPtr> additional_headers,
    network::mojom::WebSocketHandshakeClientPtr handshake_client,
    network::mojom::AuthenticationHandlerPtr auth_handler,
    network::mojom::TrustedHeaderClientPtr trusted_header_client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderProcessHost* process = RenderProcessHost::FromID(process_id);
  if (!process) {
    return;
  }
  process->GetStoragePartition()->GetNetworkContext()->CreateWebSocket(
      url, requested_protocols, site_for_cookies, std::move(additional_headers),
      process_id, frame_id, origin, options, std::move(handshake_client),
      std::move(auth_handler), std::move(trusted_header_client));
}
}  // namespace content
