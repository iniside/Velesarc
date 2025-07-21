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

#include "Items/ArcItemId.h"
#include "Tasks/TargetingTask.h"
#include "Types/TargetingSystemDataStores.h"
#include "ArcTargetingSourceContext.generated.h"

struct FArcItemData;
class AActor;
class UObject;
class UArcCoreGameplayAbility;
class UArcItemsStoreComponent;
class UArcCoreAbilitySystemComponent;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcTargetingSourceContext
{
	GENERATED_BODY()

public:
	/** Convenience method to make using the global data store easier */
	static FArcTargetingSourceContext& FindOrAdd(FTargetingRequestHandle Handle);
	static FArcTargetingSourceContext* Find(FTargetingRequestHandle Handle);

	/** The optional actor the targeting request sources from (i.e. player/projectile/etc) */
	UPROPERTY()
	TObjectPtr<AActor> SourceActor = nullptr;

	/** The optional instigator the targeting request is owned by (i.e. owner of a projectile) */
	UPROPERTY()
	TObjectPtr<AActor> InstigatorActor = nullptr;

	/** The optional location the targeting request will source from (i.e. do AOE targeting at x/y/z location) */
	UPROPERTY()
	FVector SourceLocation = FVector::ZeroVector;

	/** The optional socket name to use on the source actor (if an actor is defined) */
	UPROPERTY()
	FName SourceSocketName = NAME_None;

	/** The optional reference to a source uobject to use in the context */
	UPROPERTY()
	TObjectPtr<UObject> SourceObject = nullptr;

	UPROPERTY()
	TObjectPtr<UArcCoreGameplayAbility> SourceAbility;

	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> ItemsStoreComponent;

	UPROPERTY()
	TObjectPtr<UArcCoreAbilitySystemComponent> ArcASC;
	
	UPROPERTY(Transient)
	FArcItemId SourceItemId;

	/** @brief Item which is source of this ability. Ie item component or base item. */
	FArcItemData* SourceItemPtr = nullptr;
};

DECLARE_TARGETING_DATA_STORE(FArcTargetingSourceContext);

namespace Arcx
{
	ARCCORE_API FTargetingRequestHandle MakeTargetRequestHandle(const UTargetingPreset* TargetingPreset, const FArcTargetingSourceContext& InSourceContext);
}
