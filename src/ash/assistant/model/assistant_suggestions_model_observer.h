// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_SUGGESTIONS_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_SUGGESTIONS_MODEL_OBSERVER_H_

#include <map>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list_types.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace ash {

// A checked observer which receives notification of changes to the Assistant
// suggestions model.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantSuggestionsModelObserver
    : public base::CheckedObserver {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  // Invoked when the cache of conversation starters has changed.
  virtual void OnConversationStartersChanged(
      const std::map<int, const AssistantSuggestion*>& conversation_starters) {}

 protected:
  ~AssistantSuggestionsModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_SUGGESTIONS_MODEL_OBSERVER_H_
