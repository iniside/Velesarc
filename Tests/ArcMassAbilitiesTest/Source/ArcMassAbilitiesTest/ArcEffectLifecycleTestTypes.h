// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectTypes.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "ArcEffectLifecycleTestTypes.generated.h"

namespace ArcEffectLifecycleTest
{
	enum class EHookKind : uint8
	{
		Applied,
		Executed,
		Removed,
		StackChanged,
		TaskRemoved,
		Resolve
	};

	struct FHookRecord
	{
		EHookKind Kind = EHookKind::Applied;
		FName Label;
		FArcActiveEffectHandle Handle;
		EArcEffectRemovalReason Reason = EArcEffectRemovalReason::Removed;
		int32 OldCount = 0;
		int32 NewCount = 0;
		FMassEntityHandle Target;
		FMassEntityHandle Source;
		const UObject* SourceAbility = nullptr;
		FArcAbilityHandle SourceAbilityHandle;
		int32 ActiveEffectsNum = 0;
		bool bIncoming = false;
	};

	inline TArray<FHookRecord>& GetHookLog()
	{
		static TArray<FHookRecord> Log;
		return Log;
	}
}

USTRUCT(meta = (Hidden))
struct FArcTestComp_Recorder : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY()
	FName Label;

	virtual void OnApplied(const FArcEffectContext& Context) const override
	{
		Record(ArcEffectLifecycleTest::EHookKind::Applied, Context);
	}

	virtual void OnExecuted(const FArcEffectContext& Context) const override
	{
		Record(ArcEffectLifecycleTest::EHookKind::Executed, Context);
	}

	virtual void OnRemoved(const FArcEffectContext& Context, EArcEffectRemovalReason Reason) const override
	{
		ArcEffectLifecycleTest::FHookRecord& Entry = Record(ArcEffectLifecycleTest::EHookKind::Removed, Context);
		Entry.Reason = Reason;
	}

	virtual void OnStackChanged(const FArcEffectContext& Context, int32 OldCount, int32 NewCount) const override
	{
		ArcEffectLifecycleTest::FHookRecord& Entry = Record(ArcEffectLifecycleTest::EHookKind::StackChanged, Context);
		Entry.OldCount = OldCount;
		Entry.NewCount = NewCount;
	}

private:
	ArcEffectLifecycleTest::FHookRecord& Record(ArcEffectLifecycleTest::EHookKind Kind, const FArcEffectContext& Context) const
	{
		ArcEffectLifecycleTest::FHookRecord& Entry = ArcEffectLifecycleTest::GetHookLog().AddDefaulted_GetRef();
		Entry.Kind = Kind;
		Entry.Label = Label;
		Entry.Handle = Context.EffectHandle;
		Entry.Target = Context.TargetEntity;
		Entry.Source = Context.SourceEntity;
		Entry.SourceAbility = Context.SourceAbility;
		Entry.SourceAbilityHandle = Context.SourceAbilityHandle;
		return Entry;
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestTask_RemovalRecorder : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY()
	FName Label;

	virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) override
	{
		ArcEffectLifecycleTest::FHookRecord& Entry = ArcEffectLifecycleTest::GetHookLog().AddDefaulted_GetRef();
		Entry.Kind = ArcEffectLifecycleTest::EHookKind::TaskRemoved;
		Entry.Label = Label;
		Entry.Handle = OwningEffect.Handle;
		Entry.Reason = Reason;
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestComp_PreApply : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY()
	bool bVeto = false;

	UPROPERTY()
	FGameplayTag VetoReason;

	virtual void OnPreApplication(const FArcEffectApplyRequest& Request, const FArcEffectSpec& Spec,
	                              FArcEffectPreApplicationResult& Result) const override
	{
		if (bVeto)
		{
			Result.Veto(VetoReason);
		}
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestComp_Identity : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTagContainer Tags;

	virtual void GatherIdentityTags(FGameplayTagContainer& OutTags) const override
	{
		OutTags.AppendTags(Tags);
	}
};
