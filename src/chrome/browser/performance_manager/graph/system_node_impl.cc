// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/system_node_impl.h"

#include <algorithm>
#include <iterator>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl_operations.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

SystemNodeImpl::SystemNodeImpl(GraphImpl* graph) : TypedNodeBase(graph) {}

SystemNodeImpl::~SystemNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace performance_manager
