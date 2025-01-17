// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace blink {

void BrowserInterfaceBrokerProxy::Bind(
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker) {
  broker_ =
      mojo::Remote<blink::mojom::BrowserInterfaceBroker>(std::move(broker));
}

mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
BrowserInterfaceBrokerProxy::Reset() {
  broker_.reset();
  return broker_.BindNewPipeAndPassReceiver();
}

void BrowserInterfaceBrokerProxy::GetInterface(
    mojo::GenericPendingReceiver receiver) const {
  broker_->GetInterface(std::move(receiver));
}

void BrowserInterfaceBrokerProxy::GetInterface(
    const std::string& name,
    mojo::ScopedMessagePipeHandle pipe) const {
  GetInterface(mojo::GenericPendingReceiver(name, std::move(pipe)));
}

}  // namespace blink
