// Copyright Lukasz Baran. All Rights Reserved.

#include "StackMethods/ArcMassItemStackMethods.h"
#include "Operations/ArcMassItemOperations.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Rarity.h"
#include "Items/ArcItemScalableFloat.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityHandle.h"

namespace ArcMassItemStackMethodsInternal
{
	const FArcItemData* FindStackTargetWithSpace(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& InSpec, int32 Max)
	{
		TArray<const FArcItemData*> Candidates = ArcMassItems::FindItemsByDefinition(Mgr, Entity, StoreType, InSpec.GetItemDefinitionId());
		for (const FArcItemData* Candidate : Candidates)
		{
			if (ArcMassItems::IsSameStackKey(Candidate->Spec, InSpec) && static_cast<int32>(Candidate->GetStacks()) < Max)
			{
				return Candidate;
			}
		}
		return nullptr;
	}
}

bool FArcMassItemStackMethod_StackByType::CanStackMass(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& InSpec) const
{
	const int32 Max = FMath::FloorToInt(MaxStacks.GetValue());
	const FArcItemData* Target = ArcMassItemStackMethodsInternal::FindStackTargetWithSpace(
		Mgr, Entity, StoreType, InSpec, Max);
	return Target != nullptr;
}

FArcItemId FArcMassItemStackMethod_StackByType::StackCheckMass(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const
{
	const int32 Max = FMath::FloorToInt(MaxStacks.GetValue());
	const FArcItemData* Existing = ArcMassItemStackMethodsInternal::FindStackTargetWithSpace(
		Mgr, Entity, StoreType, InSpec, Max);
	if (!Existing)
	{
		return FArcItemId::InvalidId;
	}

	const int32 CurrentStacks = static_cast<int32>(Existing->GetStacks());
	const int32 CombinedStacks = CurrentStacks + static_cast<int32>(InSpec.Amount);

	if (CombinedStacks >= Max)
	{
		OutNewStacks = static_cast<uint16>(Max);
		OutRemainingStacks = static_cast<uint16>(CombinedStacks - Max);
	}
	else
	{
		OutNewStacks = static_cast<uint16>(CombinedStacks);
		OutRemainingStacks = 0;
	}

	return Existing->GetItemId();
}

bool FArcMassItemStackMethod_CanNotStackUnique::CanAddMass(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& InSpec) const
{
	const FArcItemData* Existing = ArcMassItems::FindItemByDefinition(Mgr, Entity, StoreType, InSpec.GetItemDefinitionId());
	return Existing == nullptr;
}
