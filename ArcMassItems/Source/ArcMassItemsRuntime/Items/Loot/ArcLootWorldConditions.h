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

#include "ArcCore/Conditions/ArcWorldConditionBase.h"
#include "GameplayTagContainer.h"
#include "ArcLootWorldConditions.generated.h"

UENUM()
enum class EArcLootContextEntity : uint8
{
	Self,
	Instigator
};

/**
 * Loot world condition: checks whether the picked entity (source or requesting)
 * has gameplay tags matching TagQuery. Reads tags from FArcOwnedTagsFragment.
 */
USTRUCT(DisplayName = "Loot: Entity Has Tag")
struct ARCMASSITEMSRUNTIME_API FArcLootWorldCondition_EntityHasTag : public FArcWorldConditionBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Default")
	EArcLootContextEntity Entity = EArcLootContextEntity::Self;

	UPROPERTY(EditAnywhere, Category = "Default")
	FGameplayTagQuery TagQuery;

	virtual bool Initialize(const UWorldConditionSchema& Schema) override;
	virtual FWorldConditionResult IsTrue(const FWorldConditionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription() const override;
#endif

private:
	FWorldConditionContextDataRef EntityRef;
	FWorldConditionContextDataRef SubsystemRef;
};
