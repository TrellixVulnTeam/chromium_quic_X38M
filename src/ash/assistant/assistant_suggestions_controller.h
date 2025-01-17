// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_H_

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/mojom/voice_interaction_controller.mojom.h"
#include "base/macros.h"

namespace ash {

class AssistantController;
class AssistantSuggestionsModelObserver;

class AssistantSuggestionsController : public AssistantControllerObserver,
                                       public AssistantUiModelObserver,
                                       public DefaultVoiceInteractionObserver {
 public:
  explicit AssistantSuggestionsController(
      AssistantController* assistant_controller);
  ~AssistantSuggestionsController() override;

  // Returns a reference to the underlying model.
  const AssistantSuggestionsModel* model() const { return &model_; }

  // Adds/removes the specified suggestions model |observer|.
  void AddModelObserver(AssistantSuggestionsModelObserver* observer);
  void RemoveModelObserver(AssistantSuggestionsModelObserver* observer);

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  // DefaultVoiceInteractionObserver:
  void OnVoiceInteractionContextEnabled(bool enabled) override;

  void UpdateConversationStarters();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  AssistantSuggestionsModel model_;

  DISALLOW_COPY_AND_ASSIGN(AssistantSuggestionsController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_H_
