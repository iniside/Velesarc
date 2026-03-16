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

#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemFragment_UseAbility.generated.h"

class UGameplayAbility;

/**
 * References an ability class to activate when this item is "used".
 * The ability must be granted to the owner's ASC externally.
 * Activation happens via SendGameplayEventToActor using ActivationEventTag.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Use Ability", Category = "Gameplay Ability", ToolTip = "References an ability class to activate when this item is used. The ability must be pre-granted to the ASC and configured with a GameplayEvent trigger matching ActivationEventTag."))
struct ARCCORE_API FArcItemFragment_UseAbility : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> UseAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	FGameplayTag ActivationEventTag;
};
