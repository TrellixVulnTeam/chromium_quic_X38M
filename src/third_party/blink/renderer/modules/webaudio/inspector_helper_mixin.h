// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_INSPECTOR_HELPER_MIXIN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_INSPECTOR_HELPER_MIXIN_H_

#include <memory>
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AudioGraphTracer;

// Supports the event reporting between Blink WebAudio module and the
// associated DevTool's inspector agent. Generates an UUID for each element,
// and keeps an UUID for a parent element.
class InspectorHelperMixin : public GarbageCollectedMixin {
 public:
  explicit InspectorHelperMixin(AudioGraphTracer&, const String& parent_uuid);
  ~InspectorHelperMixin() = default;

  AudioGraphTracer& GraphTracer() { return *graph_tracer_; }
  const String& Uuid() const { return uuid_; }
  const String& ParentUuid() const { return parent_uuid_; }

  // Called by the subclass to report the construction of graph objects
  // (BaseAudioContext, AudioNode, AudioParam, AudioListener) to the inspector
  // agent. Note that the devtools frontend will be expecting the parent object
  // to be the first in this call.
  virtual void ReportDidCreate() = 0;

  // Called by the subclass to report the construction of graph objects
  // (BaseAudioContext, AudioNode, AudioParam, AudioListener) to the inspector
  // agent. Note that the devtools frontend will be expecting the parent object
  // to be the last in this call.
  virtual void ReportWillBeDestroyed() = 0;

  void Trace(Visitor*) override;

 private:
  Member<AudioGraphTracer> graph_tracer_;
  const String uuid_;
  const String parent_uuid_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_INSPECTOR_HELPER_MIXIN_H_
