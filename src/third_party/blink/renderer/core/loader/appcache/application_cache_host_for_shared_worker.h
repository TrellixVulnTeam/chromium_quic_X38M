// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"

namespace blink {

class ApplicationCacheHostForSharedWorker final : public ApplicationCacheHost {
 public:
  ApplicationCacheHostForSharedWorker(
      const base::UnguessableToken& appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ApplicationCacheHostForSharedWorker() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_
