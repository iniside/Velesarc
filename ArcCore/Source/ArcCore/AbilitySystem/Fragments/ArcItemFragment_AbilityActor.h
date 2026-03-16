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

#include "Templates/SubclassOf.h"

#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemFragment_AbilityActor.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Holds a soft class reference to an actor spawned by abilities (e.g., projectile, area effect). Used as a value type in ability actor fragments and maps."))
struct FArcAbilityActorItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game"))
	TSoftClassPtr<AActor> AbilityActorClass;
};

/**
 * 
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Ability Actor", Category = "Gameplay Ability", ToolTip = "References an actor class spawned by abilities for this item, such as a projectile or area effect actor."))
struct ARCCORE_API FArcItemFragment_AbilityActor : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game"))
	FArcAbilityActorItem AbilityActorClass;

#if WITH_EDITOR
	virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif
};


/**
 * 
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Ability Actor Map", Category = "Gameplay Ability", ToolTip = "Maps gameplay tags to ability actor classes, allowing different actor types per ability event. Use when an item has multiple ability actors keyed by action type."))
struct ARCCORE_API FArcItemFragment_AbilityActorMap : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game", ForceInlineRow))
	TMap<FGameplayTag, FArcAbilityActorItem> AbilityActorClasses;

#if WITH_EDITOR
	virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif
};