// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

namespace blink {

CustomLayoutFragment::CustomLayoutFragment(
    CustomLayoutChild* child,
    CustomLayoutToken* token,
    scoped_refptr<const NGLayoutResult> layout_result,
    const LogicalSize& size,
    v8::Isolate* isolate)
    : child_(child),
      token_(token),
      layout_result_(std::move(layout_result)),
      inline_size_(size.inline_size.ToDouble()),
      block_size_(size.block_size.ToDouble()) {
  // TODO(crbug.com/992950): Pass constraint data through layout result.
}

const NGLayoutResult& CustomLayoutFragment::GetLayoutResult() const {
  DCHECK(layout_result_);
  return *layout_result_;
}

LayoutBox* CustomLayoutFragment::GetLayoutBox() const {
  return child_->GetLayoutBox();
}

ScriptValue CustomLayoutFragment::data(ScriptState* script_state) const {
  // "data" is *only* exposed to the LayoutWorkletGlobalScope, and we are able
  // to return the same deserialized object. We don't need to check which world
  // it is being accessed from.
  DCHECK(ExecutionContext::From(script_state)->IsLayoutWorkletGlobalScope());
  DCHECK(script_state->World().IsWorkerWorld());

  if (layout_worklet_world_v8_data_.IsEmpty())
    return ScriptValue::CreateNull(script_state);

  return ScriptValue(script_state, layout_worklet_world_v8_data_.NewLocal(
                                       script_state->GetIsolate()));
}

void CustomLayoutFragment::Trace(blink::Visitor* visitor) {
  visitor->Trace(child_);
  visitor->Trace(token_);
  visitor->Trace(layout_worklet_world_v8_data_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
