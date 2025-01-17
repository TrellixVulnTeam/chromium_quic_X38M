// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

namespace util {

MultiSourceMemoryPressureMonitor::MultiSourceMemoryPressureMonitor()
    : current_pressure_level_(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      dispatch_callback_(base::BindRepeating(
          &base::MemoryPressureListener::NotifyMemoryPressure)),
      aggregator_(this) {
  StartMetricsTimer();
}

MultiSourceMemoryPressureMonitor::~MultiSourceMemoryPressureMonitor() = default;

void MultiSourceMemoryPressureMonitor::StartMetricsTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metric_timer_.Start(
      FROM_HERE, MemoryPressureMonitor::kUMAMemoryPressureLevelPeriod,
      BindRepeating(&MemoryPressureMonitor::RecordMemoryPressure,
                    GetCurrentPressureLevel(), /* ticks = */ 1));
}

void MultiSourceMemoryPressureMonitor::StopMetricsTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metric_timer_.Stop();
}

base::MemoryPressureListener::MemoryPressureLevel
MultiSourceMemoryPressureMonitor::GetCurrentPressureLevel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_pressure_level_;
}

std::unique_ptr<MemoryPressureVoter>
MultiSourceMemoryPressureMonitor::CreateVoter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<MemoryPressureVoter>(&aggregator_);
}

void MultiSourceMemoryPressureMonitor::SetDispatchCallback(
    const DispatchCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dispatch_callback_ = callback;
}

void MultiSourceMemoryPressureMonitor::OnMemoryPressureLevelChanged(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_pressure_level_ = level;
}

void MultiSourceMemoryPressureMonitor::OnNotifyListenersRequested() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dispatch_callback_.Run(current_pressure_level_);
}

}  // namespace util
