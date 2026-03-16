/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "CoreMinimal.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcAttributesTypes.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcNeedsSurvivalAttributeSet.generated.h"

class UAbilitySystemComponent;

/**
 * Transient meta-attribute set that bridges GAS gameplay effects to ArcNeeds Mass need fragments.
 *
 * All six Add/Remove attributes are meta-attributes (no SaveGame, no replication). Gameplay effects
 * write into the Add/Remove slots; the handlers immediately drain them to zero and propagate the
 * value into the corresponding FArcHungerNeedFragment / FArcThirstNeedFragment /
 * FArcFatigueNeedFragment on the owner's Mass entity.
 *
 * The Fatigue attribute is a readable value synced from the fragment by
 * UArcNeedsFatigueInteropProcessor.
 *
 * Unlike the RPG variant, handlers perform NO stat-modifier scaling -- pure pass-through.
 */
UCLASS()
class ARCNEEDS_API UArcNeedsSurvivalAttributeSet : public UArcAttributeSet
{
	GENERATED_BODY()

public:
	UArcNeedsSurvivalAttributeSet();

	// -------------------------------------------------------------------------
	// Hunger
	// -------------------------------------------------------------------------

	UPROPERTY()
	FGameplayAttributeData AddHunger;

	UPROPERTY()
	FGameplayAttributeData RemoveHunger;

	ARC_ATTRIBUTE_ACCESSORS(UArcNeedsSurvivalAttributeSet, AddHunger);
	ARC_ATTRIBUTE_ACCESSORS(UArcNeedsSurvivalAttributeSet, RemoveHunger);

	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcNeedsSurvivalAttributeSet, AddHunger);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcNeedsSurvivalAttributeSet, RemoveHunger);

	ARC_DEFINE_ATTRIBUTE_HANDLER(AddHunger);
	ARC_DEFINE_ATTRIBUTE_HANDLER(RemoveHunger);

	// -------------------------------------------------------------------------
	// Thirst
	// -------------------------------------------------------------------------

	UPROPERTY()
	FGameplayAttributeData AddThirst;

	UPROPERTY()
	FGameplayAttributeData RemoveThirst;

	ARC_ATTRIBUTE_ACCESSORS(UArcNeedsSurvivalAttributeSet, AddThirst);
	ARC_ATTRIBUTE_ACCESSORS(UArcNeedsSurvivalAttributeSet, RemoveThirst);

	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcNeedsSurvivalAttributeSet, AddThirst);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcNeedsSurvivalAttributeSet, RemoveThirst);

	ARC_DEFINE_ATTRIBUTE_HANDLER(AddThirst);
	ARC_DEFINE_ATTRIBUTE_HANDLER(RemoveThirst);

	// -------------------------------------------------------------------------
	// Fatigue
	// -------------------------------------------------------------------------

	UPROPERTY()
	FGameplayAttributeData AddFatigue;

	UPROPERTY()
	FGameplayAttributeData RemoveFatigue;

	/** Current fatigue value synced from Mass fragment by the interop processor. */
	UPROPERTY()
	FGameplayAttributeData Fatigue;

	ARC_ATTRIBUTE_ACCESSORS(UArcNeedsSurvivalAttributeSet, AddFatigue);
	ARC_ATTRIBUTE_ACCESSORS(UArcNeedsSurvivalAttributeSet, RemoveFatigue);
	ARC_ATTRIBUTE_ACCESSORS(UArcNeedsSurvivalAttributeSet, Fatigue);

	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcNeedsSurvivalAttributeSet, AddFatigue);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcNeedsSurvivalAttributeSet, RemoveFatigue);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcNeedsSurvivalAttributeSet, Fatigue);

	ARC_DEFINE_ATTRIBUTE_HANDLER(AddFatigue);
	ARC_DEFINE_ATTRIBUTE_HANDLER(RemoveFatigue);
};
