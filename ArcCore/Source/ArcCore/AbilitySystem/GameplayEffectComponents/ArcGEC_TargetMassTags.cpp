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

#include "ArcGEC_TargetMassTags.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/ArcGameplayAbilityActorInfo.h"
#include "GameplayEffect.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "MassEntitySubsystem.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ArcGEC_TargetMassTags"

bool UArcGEC_TargetMassTags::OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const
{
	if (Tags.IsEmpty())
	{
		return true;
	}

	UAbilitySystemComponent* ASC = ActiveGEContainer.Owner;
	if (!ASC)
	{
		return true;
	}

	const FArcGameplayAbilityActorInfo* ActorInfo = static_cast<const FArcGameplayAbilityActorInfo*>(ASC->AbilityActorInfo.Get());
	if (!ActorInfo || !ActorInfo->EntityHandle.IsValid() || !ActorInfo->MassEntitySubsystem)
	{
		return true;
	}

	FMassEntityHandle Entity = ActorInfo->EntityHandle;
	FMassEntityManager& EntityManager = ActorInfo->MassEntitySubsystem->GetMutableEntityManager();

	for (const FInstancedStruct& TagInstance : Tags)
	{
		const UScriptStruct* TagType = TagInstance.GetScriptStruct();
		if (!TagType)
		{
			continue;
		}

		EntityManager.Defer().PushCommand<FMassDeferredAddCommand>([Entity, TagType](FMassEntityManager& Mgr)
		{
			FStructView _ = Mgr.AddSparseElementToEntity(Entity, TagType);
		});
	}

	ActiveGE.EventSet.OnEffectRemoved.AddUObject(this, &UArcGEC_TargetMassTags::OnActiveGameplayEffectRemoved);

	return true;
}

void UArcGEC_TargetMassTags::OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo) const
{
	UAbilitySystemComponent* ASC = RemovalInfo.ActiveEffect->Handle.GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const FArcGameplayAbilityActorInfo* ActorInfo = static_cast<const FArcGameplayAbilityActorInfo*>(ASC->AbilityActorInfo.Get());
	if (!ActorInfo || !ActorInfo->EntityHandle.IsValid() || !ActorInfo->MassEntitySubsystem)
	{
		return;
	}

	FMassEntityHandle Entity = ActorInfo->EntityHandle;
	FMassEntityManager& EntityManager = ActorInfo->MassEntitySubsystem->GetMutableEntityManager();

	for (const FInstancedStruct& TagInstance : Tags)
	{
		const UScriptStruct* TagType = TagInstance.GetScriptStruct();
		if (!TagType)
		{
			continue;
		}

		EntityManager.Defer().PushCommand<FMassDeferredRemoveCommand>([Entity, TagType](FMassEntityManager& Mgr)
		{
			Mgr.RemoveSparseElementFromEntity(Entity, TagType);
		});
	}
}

#if WITH_EDITOR
EDataValidationResult UArcGEC_TargetMassTags::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const bool bInstantEffect = (GetOwner()->DurationPolicy == EGameplayEffectDurationType::Instant);
	if (bInstantEffect && !Tags.IsEmpty())
	{
		Context.AddError(FText::FormatOrdered(
			LOCTEXT("GEInstantAndMassTags", "GE {0} is set to Instant so ArcGEC_TargetMassTags cannot add/remove Mass tags."),
			FText::FromString(GetNameSafe(GetOwner()))));
		Result = EDataValidationResult::Invalid;
	}

	if (Tags.IsEmpty())
	{
		Context.AddWarning(FText::FormatOrdered(
			LOCTEXT("GENoMassTags", "GE {0} has ArcGEC_TargetMassTags with no tags configured."),
			FText::FromString(GetNameSafe(GetOwner()))));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
