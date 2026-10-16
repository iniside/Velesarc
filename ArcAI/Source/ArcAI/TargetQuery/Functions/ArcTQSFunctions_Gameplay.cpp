// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSFunctions_Gameplay.h"

#include "Mass/EntityManager.h"
#include "Memory/ArcMassMemory.h"
#include "Perception/ArcMassHearingPerception.h"
#include "Perception/ArcMassSightPerception.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSFunctions_Gameplay)

namespace ArcTQSFunctions_Private
{
	static bool FindPerceived(const FArcMassPerceptionResultFragmentBase* Fragment, FMassEntityHandle Entity, FVector& OutLocation)
	{
		if (!Fragment)
		{
			return false;
		}
		const FArcPerceivedEntity* Found = Fragment->PerceivedEntities.FindByPredicate([Entity](const FArcPerceivedEntity& Perceived)
		{
			return Perceived.Entity == Entity;
		});
		if (!Found)
		{
			return false;
		}
		OutLocation = Found->LastKnownLocation;
		return true;
	}

	template<typename T>
	void ReadMemory(const FArcTQSFunctionContext& Context, const FGameplayTag Key, const T& Fallback, T& Result)
	{
		Result = Fallback;
		if (Context.EntityManager && Context.Self.IsValid())
		{
			ArcMassMemory::Read(*Context.EntityManager, Context.Self, Key, Result);
		}
	}
}

void FArcTQSFunction_PerceivedLastLocation::Execute(const FArcTQSFunctionContext& Context)
{
	Result = Fallback;
	FMassEntityManager* EntityManager = Context.EntityManager;
	if (!EntityManager || !Perceiver.IsValid() || !Entity.IsValid() || !EntityManager->IsEntityActive(Perceiver))
	{
		return;
	}

	if (!ArcTQSFunctions_Private::FindPerceived(EntityManager->GetFragmentDataPtr<FArcMassSightPerceptionResult>(Perceiver), Entity, Result))
	{
		ArcTQSFunctions_Private::FindPerceived(EntityManager->GetFragmentDataPtr<FArcMassHearingPerceptionResult>(Perceiver), Entity, Result);
	}
}

void FArcTQSFunction_MemoryLocation::Execute(const FArcTQSFunctionContext& Context)
{
	ArcTQSFunctions_Private::ReadMemory(Context, Key, Fallback, Result);
}

void FArcTQSFunction_MemoryEntity::Execute(const FArcTQSFunctionContext& Context)
{
	ArcTQSFunctions_Private::ReadMemory(Context, Key, Fallback, Result);
}

void FArcTQSFunction_MemoryFloat::Execute(const FArcTQSFunctionContext& Context)
{
	ArcTQSFunctions_Private::ReadMemory(Context, Key, Fallback, Result);
}

void FArcTQSFunction_MemoryBool::Execute(const FArcTQSFunctionContext& Context)
{
	ArcTQSFunctions_Private::ReadMemory(Context, Key, bFallback, bResult);
}

void FArcTQSFunction_MemoryTag::Execute(const FArcTQSFunctionContext& Context)
{
	ArcTQSFunctions_Private::ReadMemory(Context, Key, Fallback, Result);
}
