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



#include "ArcQuickSlotHandler_OverrideInputMappingContext.h"

#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "QuickBar/Fragments/ArcItemFragment_InputMappingContext.h"

void FArcQuickSlotHandler_OverrideInputMappingContext::OnSlotSelected(UArcCoreAbilitySystemComponent* InArcASC
																	  , UArcQuickBarComponent* QuickBar
																	  , const FArcItemData* InSlot
																	  , const FArcQuickBar* InQuickBar
								, const struct FArcQuickBarSlot* InQuickSlot) const
{
	APlayerState* PS = InArcASC->GetOwner<APlayerState>();

	const APawn* Pawn = PS->GetPawn();

	const APlayerController* PC = Pawn->GetController<APlayerController>();
	check(PC);

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	UInputMappingContext* MappingContext = InputMappingContext;

	const FArcItemFragment_InputMappingContext* Fragment = ArcItemsHelper::GetFragment<FArcItemFragment_InputMappingContext>(InSlot);
	if (Fragment)
	{
		MappingContext = Fragment->InputMappingContext;
	}
	FModifyContextOptions Context;
	Context.bForceImmediately = true;
	Context.bIgnoreAllPressedKeysUntilRelease = false;
	Subsystem->AddMappingContext(MappingContext
		, Priority
		, Context);
}

void FArcQuickSlotHandler_OverrideInputMappingContext::OnSlotDeselected(UArcCoreAbilitySystemComponent* InArcASC
																		, UArcQuickBarComponent* QuickBar
																		, const FArcItemData* InSlot
																		, const FArcQuickBar* InQuickBar
								, const struct FArcQuickBarSlot* InQuickSlot) const
{
	APlayerState* PS = InArcASC->GetOwner<APlayerState>();

	const APawn* Pawn = PS->GetPawn();

	const APlayerController* PC = Pawn->GetController<APlayerController>();
	if (PC == nullptr)
	{
		return;
	}
	check(PC);

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	UInputMappingContext* MappingContext = InputMappingContext;

	const FArcItemFragment_InputMappingContext* Fragment = ArcItemsHelper::GetFragment<
		FArcItemFragment_InputMappingContext>(InSlot);
	if (Fragment)
	{
		MappingContext = Fragment->InputMappingContext;
	}

	FModifyContextOptions Context;
	Context.bForceImmediately = true;
	Context.bIgnoreAllPressedKeysUntilRelease = false;
	Subsystem->RemoveMappingContext(MappingContext
		, Context);
}