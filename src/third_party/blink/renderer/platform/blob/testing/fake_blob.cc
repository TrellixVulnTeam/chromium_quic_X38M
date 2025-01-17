// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"

namespace blink {
namespace {

class SimpleDataPipeGetter : public network::mojom::blink::DataPipeGetter {
 public:
  SimpleDataPipeGetter(const String& str) : str_(str) {}
  ~SimpleDataPipeGetter() override = default;

  // network::mojom::DataPipeGetter implementation:
  void Read(mojo::ScopedDataPipeProducerHandle handle,
            ReadCallback callback) override {
    std::move(callback).Run(0 /* OK */, str_.length());
    bool result = mojo::BlockingCopyFromString(str_.Utf8(), handle);
    DCHECK(result);
  }

  void Clone(network::mojom::blink::DataPipeGetterRequest request) override {
    mojo::MakeStrongBinding(std::make_unique<SimpleDataPipeGetter>(str_),
                            std::move(request));
  }

 private:
  String str_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDataPipeGetter);
};

}  // namespace

FakeBlob::FakeBlob(const String& uuid, const String& body, State* state)
    : uuid_(uuid), body_(body), state_(state) {}

void FakeBlob::Clone(mojom::blink::BlobRequest request) {
  mojo::MakeStrongBinding(std::make_unique<FakeBlob>(uuid_, body_, state_),
                          std::move(request));
}

void FakeBlob::AsDataPipeGetter(
    network::mojom::blink::DataPipeGetterRequest request) {
  if (state_)
    state_->did_initiate_read_operation = true;
  mojo::MakeStrongBinding(std::make_unique<SimpleDataPipeGetter>(body_),
                          std::move(request));
}

void FakeBlob::ReadRange(uint64_t offset,
                         uint64_t length,
                         mojo::ScopedDataPipeProducerHandle,
                         mojo::PendingRemote<mojom::blink::BlobReaderClient>) {
  NOTREACHED();
}

void FakeBlob::ReadAll(
    mojo::ScopedDataPipeProducerHandle handle,
    mojo::PendingRemote<mojom::blink::BlobReaderClient> client) {
  mojo::Remote<mojom::blink::BlobReaderClient> client_remote(std::move(client));
  if (state_)
    state_->did_initiate_read_operation = true;
  if (client_remote)
    client_remote->OnCalculatedSize(body_.length(), body_.length());
  bool result = mojo::BlockingCopyFromString(body_.Utf8(), handle);
  DCHECK(result);
  if (client_remote)
    client_remote->OnComplete(0 /* OK */, body_.length());
}

void FakeBlob::ReadSideData(ReadSideDataCallback callback) {
  NOTREACHED();
}

void FakeBlob::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(uuid_);
}

}  // namespace blink
