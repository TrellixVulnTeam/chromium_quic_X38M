// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_FAKE_EMBEDDED_WORKER_INSTANCE_CLIENT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_FAKE_EMBEDDED_WORKER_INSTANCE_CLIENT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"

namespace content {

class EmbeddedWorkerTestHelper;

// The default fake for blink::mojom::EmbeddedWorkerInstanceClient. It responds
// to Start/Stop/etc messages without starting an actual service worker thread.
// It is owned by EmbeddedWorkerTestHelper and by default the lifetime is tied
// to the Mojo connection.
class FakeEmbeddedWorkerInstanceClient
    : public blink::mojom::EmbeddedWorkerInstanceClient {
 public:
  // |helper| must outlive this instance.
  explicit FakeEmbeddedWorkerInstanceClient(EmbeddedWorkerTestHelper* helper);
  ~FakeEmbeddedWorkerInstanceClient() override;

  EmbeddedWorkerTestHelper* helper() { return helper_; }

  base::WeakPtr<FakeEmbeddedWorkerInstanceClient> GetWeakPtr();

  blink::mojom::EmbeddedWorkerInstanceHostAssociatedPtr& host() {
    return host_;
  }

  void Bind(blink::mojom::EmbeddedWorkerInstanceClientRequest request);
  void RunUntilBound();

  // Closes the binding and deletes |this|.
  void Disconnect();

 protected:
  // blink::mojom::EmbeddedWorkerInstanceClient implementation.
  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override;
  void StopWorker() override;
  void ResumeAfterDownload() override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override {}

  virtual void EvaluateScript();

  void DidPopulateScriptCacheMap();

  blink::mojom::EmbeddedWorkerStartParamsPtr& start_params() {
    return start_params_;
  }

  void OnConnectionError();

 private:
  // |helper_| owns |this|.
  EmbeddedWorkerTestHelper* const helper_;

  blink::mojom::EmbeddedWorkerStartParamsPtr start_params_;
  blink::mojom::EmbeddedWorkerInstanceHostAssociatedPtr host_;

  mojo::Binding<blink::mojom::EmbeddedWorkerInstanceClient> binding_;
  base::OnceClosure quit_closure_for_bind_;

  base::WeakPtrFactory<FakeEmbeddedWorkerInstanceClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeEmbeddedWorkerInstanceClient);
};

// A EmbeddedWorkerInstanceClient fake that doesn't respond to the Start/Stop
// message until instructed to do so.
class DelayedFakeEmbeddedWorkerInstanceClient
    : public FakeEmbeddedWorkerInstanceClient {
 public:
  explicit DelayedFakeEmbeddedWorkerInstanceClient(
      EmbeddedWorkerTestHelper* helper);
  ~DelayedFakeEmbeddedWorkerInstanceClient() override;

  // Unblocks the Start/StopWorker() call to this instance. May be called before
  // or after the Start/StopWorker() call.
  void UnblockStartWorker();
  void UnblockStopWorker();

  // Returns after Start/StopWorker() is called.
  void RunUntilStartWorker();
  void RunUntilStopWorker();

 protected:
  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override;
  void StopWorker() override;

 private:
  void CompleteStopWorker();

  enum class State { kWillBlock, kWontBlock, kBlocked, kCompleted };
  State start_state_ = State::kWillBlock;
  State stop_state_ = State::kWillBlock;
  base::OnceClosure quit_closure_for_start_worker_;
  base::OnceClosure quit_closure_for_stop_worker_;

  // Valid after StartWorker() until start is unblocked.
  blink::mojom::EmbeddedWorkerStartParamsPtr start_params_;

  DISALLOW_COPY_AND_ASSIGN(DelayedFakeEmbeddedWorkerInstanceClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_FAKE_EMBEDDED_WORKER_INSTANCE_CLIENT_H_
