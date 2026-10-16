// Copyright Lukasz Baran. All Rights Reserved.

#include "Memory/ArcMassMemory.h"

#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "StructUtils/StructView.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassMemory, Log, All);

UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Memory_Anchor, "AI.Memory.Anchor");
UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Memory_PatrolPoint, "AI.Memory.PatrolPoint");

namespace ArcMassMemoryPrivate
{
	using FMismatchKey = TTuple<FGameplayTag, const UScriptStruct*, const UScriptStruct*>;

	TSet<FMismatchKey>& GetReportedMismatches()
	{
		static TSet<FMismatchKey> Reported;
		return Reported;
	}
}

namespace ArcMassMemory
{
	FArcMassMemoryFragment* FindMutableFragment(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			return nullptr;
		}
		FStructView View = EntityManager.GetMutableSparseElementDataForEntity(FArcMassMemoryFragment::StaticStruct(), Entity);
		return View.GetPtr<FArcMassMemoryFragment>();
	}

	const FArcMassMemoryFragment* FindFragment(const FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			return nullptr;
		}
		FConstStructView View = EntityManager.GetSparseElementDataForEntity(FArcMassMemoryFragment::StaticStruct(), Entity);
		return View.GetPtr<const FArcMassMemoryFragment>();
	}

	void WarnTypeMismatch(const FGameplayTag Key, const UScriptStruct* StoredType, const UScriptStruct* RequestedType)
	{
		check(IsInGameThread());
		bool bAlreadyReported = false;
		ArcMassMemoryPrivate::GetReportedMismatches().Add(
			ArcMassMemoryPrivate::FMismatchKey(Key, StoredType, RequestedType), &bAlreadyReported);
		if (!bAlreadyReported)
		{
			UE_LOGF(LogArcMassMemory, Warning, "NPC memory key %ls holds %ls but was read as %ls.",
				*Key.ToString(), *GetNameSafe(StoredType), *GetNameSafe(RequestedType));
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	void ResetReportedMismatchesForTesting()
	{
		check(IsInGameThread());
		ArcMassMemoryPrivate::GetReportedMismatches().Reset();
	}
#endif

	void Forget(FArcMassMemoryFragment& Fragment, const FGameplayTag Key)
	{
		Fragment.Values.Remove(Key);
	}

	bool Forget(FMassEntityManager& EntityManager, const FMassEntityHandle Entity, const FGameplayTag Key)
	{
		FArcMassMemoryFragment* Fragment = FindMutableFragment(EntityManager, Entity);
		if (!Fragment)
		{
			return false;
		}
		Forget(*Fragment, Key);
		return true;
	}
}

void UArcMassMemoryTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	Out.AddSparseFragment<FArcMassMemoryFragment>();
}

void UArcMassMemoryTrait::InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const
{
	FStructView View = Mgr.GetMutableSparseElementDataForEntity(FArcMassMemoryFragment::StaticStruct(), Entity);
	FArcMassMemoryFragment* Fragment = View.GetPtr<FArcMassMemoryFragment>();
	if (Fragment == nullptr)
	{
		return;
	}

	const FMassEntityView EntityView(Mgr, Entity);
	const FTransformFragment* TransformFragment = EntityView.GetFragmentDataPtr<FTransformFragment>();
	if (TransformFragment == nullptr)
	{
		return;
	}

	ArcMassMemory::Write(*Fragment, TAG_AI_Memory_Anchor, TransformFragment->GetTransform().GetLocation());
}
