// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_CUSTOM_LAYOUT_CHILD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_CUSTOM_LAYOUT_CHILD_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CSSLayoutDefinition;
class CustomLayoutConstraintsOptions;
class CustomLayoutToken;
class LayoutBox;

// Represents a "CSS box" for use by a web developer. This is passed into the
// web developer defined layout and intrinsicSizes functions so that they can
// perform layout on these children.
//
// The represent all inflow children, out-of-flow children (fixed/absolute) do
// not appear in the children list.
class CustomLayoutChild : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CustomLayoutChild(const CSSLayoutDefinition&, LayoutBox*);
  ~CustomLayoutChild() override = default;

  // LayoutChild.idl
  PrepopulatedComputedStylePropertyMap* styleMap() const { return style_map_; }
  ScriptPromise layoutNextFragment(ScriptState*,
                                   const CustomLayoutConstraintsOptions*,
                                   ExceptionState&);

  LayoutBox* GetLayoutBox() const {
    DCHECK(box_);
    return box_;
  }
  void ClearLayoutBox() { box_ = nullptr; }

  void SetCustomLayoutToken(CustomLayoutToken* token) { token_ = token; }

  void Trace(blink::Visitor*) override;

 private:
  LayoutBox* box_;
  Member<PrepopulatedComputedStylePropertyMap> style_map_;
  Member<CustomLayoutToken> token_;

  DISALLOW_COPY_AND_ASSIGN(CustomLayoutChild);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_CUSTOM_LAYOUT_CHILD_H_
