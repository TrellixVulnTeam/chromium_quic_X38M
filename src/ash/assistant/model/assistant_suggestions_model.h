// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_SUGGESTIONS_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_SUGGESTIONS_MODEL_H_

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace ash {

class AssistantSuggestionsModelObserver;

class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantSuggestionsModel {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;

  AssistantSuggestionsModel();
  ~AssistantSuggestionsModel();

  // Adds/removes the specified suggestions model |observer|.
  void AddObserver(AssistantSuggestionsModelObserver* observer);
  void RemoveObserver(AssistantSuggestionsModelObserver* observer);

  // Sets the cache of conversation starters.
  void SetConversationStarters(
      std::vector<AssistantSuggestionPtr> conversation_starters);

  // Returns the conversation starter uniquely identified by |id|.
  const AssistantSuggestion* GetConversationStarterById(int id) const;

  // Returns all cached conversation starters, mapped to a unique id.
  std::map<int, const AssistantSuggestion*> GetConversationStarters() const;

 private:
  void NotifyConversationStartersChanged();

  std::vector<AssistantSuggestionPtr> conversation_starters_;

  base::ObserverList<AssistantSuggestionsModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantSuggestionsModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_SUGGESTIONS_MODEL_H_
