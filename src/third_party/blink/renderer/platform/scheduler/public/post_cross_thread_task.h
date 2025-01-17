// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_POST_CROSS_THREAD_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_POST_CROSS_THREAD_TASK_H_

#include <utility>
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// For cross-thread posting. Can be called from any thread.
inline void PostCrossThreadTask(base::SequencedTaskRunner& task_runner,
                                const base::Location& location,
                                WTF::CrossThreadOnceClosure task) {
  task_runner.PostDelayedTask(
      location, ConvertToBaseOnceCallback(std::move(task)), base::TimeDelta());
}

inline void PostDelayedCrossThreadTask(base::SequencedTaskRunner& task_runner,
                                       const base::Location& location,
                                       WTF::CrossThreadOnceClosure task,
                                       base::TimeDelta delay) {
  task_runner.PostDelayedTask(
      location, ConvertToBaseOnceCallback(std::move(task)), delay);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_POST_CROSS_THREAD_TASK_H_
