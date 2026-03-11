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

#pragma once

#include "Items/Fragments/ArcItemFragment.h"
#include "AbilitySystem/ArcAbilityAction.h"
#include "Engine/DataAsset.h"
#include "ArcItemFragment_AbilityActions.generated.h"

UCLASS()
class ARCCORE_API UArcAbilityActionsPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Events", meta = (Categories = "Ability.Event,GameplayEvent.Ability", ForceInlineRow))
	TMap<FGameplayTag, FArcAbilityActionList> EventActions;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> OnLocalTargetResultActions;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> OnAbilityTargetResultActions;

	UPROPERTY(EditAnywhere, Category = "Lifecycle", meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> OnActivateActions;

	UPROPERTY(EditAnywhere, Category = "Lifecycle", meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> OnEndActions;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Ability Actions", Category = "Gameplay Ability", ToolTip = "Defines data-driven ability actions for this item. Maps gameplay event tags to action lists and provides hooks for targeting results, activation, and end lifecycle events. Actions are FArcAbilityAction subclasses composed via FInstancedStruct. Can reference a shared UArcAbilityActionsPreset data asset or define actions inline."))
struct ARCCORE_API FArcItemFragment_AbilityActions : public FArcItemFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Preset")
	TObjectPtr<UArcAbilityActionsPreset> Preset;

	UPROPERTY(EditAnywhere, Category = "Events", meta = (Categories = "Ability.Event,GameplayEvent.Ability", ForceInlineRow))
	TMap<FGameplayTag, FArcAbilityActionList> EventActions;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> OnLocalTargetResultActions;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> OnAbilityTargetResultActions;

	UPROPERTY(EditAnywhere, Category = "Lifecycle", meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> OnActivateActions;

	UPROPERTY(EditAnywhere, Category = "Lifecycle", meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> OnEndActions;

public:
	const TMap<FGameplayTag, FArcAbilityActionList>& GetEventActions() const
	{
		if (Preset) { return Preset->EventActions; }
		return EventActions;
	}

	const TArray<FInstancedStruct>& GetLocalTargetResultActions() const
	{
		if (Preset) { return Preset->OnLocalTargetResultActions; }
		return OnLocalTargetResultActions;
	}

	const TArray<FInstancedStruct>& GetAbilityTargetResultActions() const
	{
		if (Preset) { return Preset->OnAbilityTargetResultActions; }
		return OnAbilityTargetResultActions;
	}

	const TArray<FInstancedStruct>& GetActivateActions() const
	{
		if (Preset) { return Preset->OnActivateActions; }
		return OnActivateActions;
	}

	const TArray<FInstancedStruct>& GetEndActions() const
	{
		if (Preset) { return Preset->OnEndActions; }
		return OnEndActions;
	}
};
