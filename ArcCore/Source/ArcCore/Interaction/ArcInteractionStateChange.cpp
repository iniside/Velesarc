/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */



#include "ArcInteractionStateChange.h"

#include "ArcInteractionRepresentation.h"
#include "ArcInteractionStateChangePreset.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "ArcStartInteractionCommand.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Interaction_Show_Message
	, "GameplayMessage.Interaction.Show");
UE_DEFINE_GAMEPLAY_TAG(TAG_Interaction_Hide_Message
	, "GameplayMessage.Interaction.Hide");


void FArcInteractionStateChange_ShowHUDWidget::OnSelected(APawn* InPawn
														  , AController* InController
														  , const UArcInteractionReceiverComponent* InReceiver
														  , TScriptInterface<IArcInteractionObjectInterface> InteractionManager
														  , const FGuid& InteractionId) const
{
	if (InPawn->IsLocallyControlled())
	{
		FArcShowPromptMessage ShowPromptMessage;
		ShowPromptMessage.WidgetClass = WidgetClass;
		ShowPromptMessage.ViewModelClasses = ViewModelClasses;
		ShowPromptMessage.PromptSlot = PromptWidgetSlot;
		ShowPromptMessage.ReceiverComponent = const_cast<UArcInteractionReceiverComponent*>(InReceiver);
		ShowPromptMessage.InteractionId = InteractionId;
		ShowPromptMessage.InteractedObject = InteractionManager;
		
		UGameplayMessageSubsystem::Get(InPawn->GetWorld()).BroadcastMessage(TAG_Interaction_Show_Message
			, ShowPromptMessage);
	}
}

void FArcInteractionStateChange_ShowHUDWidget::OnDeselected(APawn* InPawn
	, AController* InController
	, const UArcInteractionReceiverComponent* InReceiver
	, TScriptInterface<IArcInteractionObjectInterface> InteractionManager
	, const FGuid& InteractionId) const
{
	if (InPawn->IsLocallyControlled())
	{
		FArcHidePromptMessage HidePromptMessage;
		UGameplayMessageSubsystem::Get(InPawn->GetWorld()).BroadcastMessage(TAG_Interaction_Hide_Message
			, HidePromptMessage);
	}
}

void FArcInteractionStateChange_AutoPickItemRemoveInteraction::OnSelected(APawn* InPawn
	, AController* InController
	, const UArcInteractionReceiverComponent* InReceiver
	, TScriptInterface<IArcInteractionObjectInterface> InteractionManager
	, const FGuid& InteractionId) const
{
	UArcInteractionStateChangePreset* DataPreset = InteractionManager->GetInteractionStatePreset();

	const FArcInteractionData_TargetItemsStore* ItemsStoreDAta = DataPreset->FindCustomData<FArcInteractionData_TargetItemsStore>();

	APlayerState* PS = InController->GetPlayerState<APlayerState>();
	
	UArcItemsStoreComponent* TargetItemsStore = Cast<UArcItemsStoreComponent>(PS->FindComponentByClass(ItemsStoreDAta->TargetItemStoreClass));
	
	Arcx::SendServerCommand<FArcPickAllItemsAndRemoveInteractionCommand>(InController, TargetItemsStore
					, InteractionId);	
}
