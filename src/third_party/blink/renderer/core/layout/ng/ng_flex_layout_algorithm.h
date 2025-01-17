// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLEX_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLEX_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;

class CORE_EXPORT NGFlexLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGFlexLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;

  base::Optional<MinMaxSize> ComputeMinMaxSize(
      const MinMaxSizeInput&) const override;

 private:
  bool IsItemCrossSizeAuto(const NGBlockNode& child) const;
  bool ShouldItemShrinkToFit(const NGBlockNode& child) const;
  bool DoesItemStretch(const NGBlockNode& child) const;
  bool IsColumnContainerMainSizeDefinite() const;
  bool IsContainerCrossSizeDefinite() const;

  void ConstructAndAppendFlexItems();
  void GiveLinesAndItemsFinalPositionAndSize();
  // This is same method as FlexItem but we need that logic before FlexItem is
  // constructed.
  bool MainAxisIsInlineAxis(const NGBlockNode& child) const;
  LayoutUnit MainAxisContentExtent(LayoutUnit sum_hypothetical_main_size);

  void HandleOutOfFlowPositioned(NGBlockNode child);
  // TODO(dgrogan): This is redundant with FlexLayoutAlgorithm.IsMultiline() but
  // it's needed before the algorithm is instantiated. Figure out how to
  // not reimplement.
  bool IsMultiline() const;

  const NGBoxStrut border_padding_;
  const NGBoxStrut border_scrollbar_padding_;
  const bool is_column_;
  LogicalSize border_box_size_;
  LogicalSize content_box_size_;
  // These are populated at the top of Layout(), so aren't available in
  // ComputeMinMaxSize() or anything it calls.
  base::Optional<FlexLayoutAlgorithm> algorithm_;
  bool is_horizontal_flow_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLEX_LAYOUT_ALGORITHM_H_
