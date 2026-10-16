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

#include "Items/Loot/ArcLootWorldConditions.h"
#include "Items/Loot/ArcLootLogs.h"
#include "Items/Loot/ArcLootWorldConditionSchema.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "WorldConditionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLootWorldConditions)

bool FArcLootWorldCondition_EntityHasTag::Initialize(const UWorldConditionSchema& Schema)
{
	const UArcLootWorldConditionSchema* LootSchema = Cast<UArcLootWorldConditionSchema>(&Schema);
	if (LootSchema == nullptr)
	{
		return false;
	}
	EntityRef = (Entity == EArcLootContextEntity::Self)
		? LootSchema->GetSelfEntityRef()
		: LootSchema->GetInstigatorEntityRef();
	SubsystemRef = LootSchema->GetMassEntitySubsystemRef();
	bCanCacheResult = false;
	return true;
}

FWorldConditionResult FArcLootWorldCondition_EntityHasTag::IsTrue(const FWorldConditionContext& Context) const
{
	FWorldConditionResult Result(EWorldConditionResultValue::IsFalse, bCanCacheResult);

	FMassEntityHandle* EntityPtr = Context.GetContextDataPtr<FMassEntityHandle>(EntityRef);
	UMassEntitySubsystem* Subsystem = Context.GetContextDataPtr<UMassEntitySubsystem>(SubsystemRef);
	if (EntityPtr == nullptr || !EntityPtr->IsValid() || Subsystem == nullptr)
	{
		UE_LOGF(LogArcLoot, Verbose,"EntityHasTag: %ls slot null/invalid — FAIL",
			Entity == EArcLootContextEntity::Self ? TEXT("Self") : TEXT("Instigator"));
		return Result;
	}

	FMassEntityManager& EM = Subsystem->GetMutableEntityManager();
	if (!EM.IsEntityValid(*EntityPtr))
	{
		UE_LOGF(LogArcLoot, Verbose,"EntityHasTag: %ls entity handle not valid in EntityManager — FAIL",
			Entity == EArcLootContextEntity::Self ? TEXT("Self") : TEXT("Instigator"));
		return Result;
	}

	const FArcOwnedTagsFragment* TagsFragment = EM.GetFragmentDataPtr<FArcOwnedTagsFragment>(*EntityPtr);
	if (TagsFragment == nullptr)
	{
		UE_LOGF(LogArcLoot, Verbose,"EntityHasTag: entity has no FArcOwnedTagsFragment — FAIL");
		return Result;
	}

	const bool bMatch = TagQuery.Matches(TagsFragment->GetTags().GetTagContainer());
	UE_LOGF(LogArcLoot, Verbose,"EntityHasTag: %ls entity %ls match=%ls, owned tags=[%ls], query=%ls",
		Entity == EArcLootContextEntity::Self ? TEXT("Self") : TEXT("Instigator"),
		*EntityPtr->DebugGetDescription(),
		bMatch ? TEXT("TRUE") : TEXT("FALSE"),
		*TagsFragment->GetTags().GetTagContainer().ToStringSimple(),
		*TagQuery.GetDescription());

	if (bMatch)
	{
		Result.Value = EWorldConditionResultValue::IsTrue;
	}
	return Result;
}

#if WITH_EDITOR
FText FArcLootWorldCondition_EntityHasTag::GetDescription() const
{
	const FString EntityLabel = (Entity == EArcLootContextEntity::Self)
		? TEXT("Self") : TEXT("Instigator");
	const FString TagDesc = TagQuery.IsEmpty()
		? TEXT("(empty query)") : TagQuery.GetDescription();
	return FText::Format(
		NSLOCTEXT("ArcLoot", "EntityHasTagDesc", "{0} entity matches tag query: {1}"),
		FText::FromString(EntityLabel),
		FText::FromString(TagDesc));
}
#endif
