// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcAttributeCapture.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Effects/ArcEffectDefinition.h"
#include "Mass/EntityView.h"
#include "Mass/EntityManager.h"
#include "ArcMassAbilitiesTestTypes.generated.h"

USTRUCT(meta = (Hidden))
struct FArcAttributeSet_TestStats : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Health = { 100.f, 100.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute Armor = { 0.f, 0.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute Intelligence = { 10.f, 10.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute AttackPower = { 0.f, 0.f };

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_TestStats, Health)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_TestStats, Armor)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_TestStats, Intelligence)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_TestStats, AttackPower)
};

template<>
struct TMassFragmentTraits<FArcAttributeSet_TestStats> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT(meta = (Hidden))
struct FArcAttributeSet_TestDerivedStats : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxEnergy = { 0.f, 0.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute Damage = { 0.f, 0.f };

	static FArcAttributeRef GetMaxEnergyAttribute()
	{
		static FArcAttributeRef Ref = []()
		{
			FArcAttributeRef R;
			R.FragmentType = FArcAttributeSet_TestDerivedStats::StaticStruct();
			R.PropertyName = GET_MEMBER_NAME_CHECKED(FArcAttributeSet_TestDerivedStats, MaxEnergy);
			return R;
		}();
		return Ref;
	}

	static FArcAttributeRef GetDamageAttribute()
	{
		static FArcAttributeRef Ref = []()
		{
			FArcAttributeRef R;
			R.FragmentType = FArcAttributeSet_TestDerivedStats::StaticStruct();
			R.PropertyName = GET_MEMBER_NAME_CHECKED(FArcAttributeSet_TestDerivedStats, Damage);
			return R;
		}();
		return Ref;
	}
};

template<>
struct TMassFragmentTraits<FArcAttributeSet_TestDerivedStats> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT(meta = (Hidden))
struct FArcTestAddExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcAttributeRef TargetAttribute;
	float Value = 0.f;
	EArcEffectOutputSide Side = EArcEffectOutputSide::Target;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers
	) const override
	{
		FArcEffectModifierOutput Output;
		Output.Attribute = TargetAttribute;
		Output.Operation = EArcModifierOp::Add;
		Output.Magnitude = Value * Context.GetStackCount();
		Output.Side = Side;
		OutModifiers.Add(Output);
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestOverrideExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcAttributeRef TargetAttribute;
	float Value = 0.f;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers
	) const override
	{
		FArcEffectModifierOutput Output;
		Output.Attribute = TargetAttribute;
		Output.Operation = EArcModifierOp::Override;
		Output.Magnitude = Value;
		OutModifiers.Add(Output);
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestHighestAdditivePolicy : public FArcAggregationPolicy
{
	GENERATED_BODY()

	virtual float Aggregate(
		const FArcAggregationContext& Context,
		const TArray<FArcAggregatorMod> (&Mods)[static_cast<uint8>(EArcModifierOp::Max)]
	) const override
	{
		float Result = Context.BaseValue;

		float HighestAdd = -TNumericLimits<float>::Max();
		bool bFoundAdd = false;
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::Add)])
		{
			if (Mod.bQualified && Mod.Magnitude > HighestAdd)
			{
				HighestAdd = Mod.Magnitude;
				bFoundAdd = true;
			}
		}
		if (bFoundAdd)
		{
			Result += HighestAdd;
		}

		float SumMulAdd = 0.f;
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)])
		{
			if (Mod.bQualified) SumMulAdd += Mod.Magnitude;
		}
		Result *= (1.f + SumMulAdd);

		float SumDivAdd = 0.f;
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::DivideAdditive)])
		{
			if (Mod.bQualified) SumDivAdd += Mod.Magnitude;
		}
		float Divisor = 1.f + SumDivAdd;
		if (Divisor != 0.f) Result /= Divisor;

		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::MultiplyCompound)])
		{
			if (Mod.bQualified) Result *= Mod.Magnitude;
		}
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::AddFinal)])
		{
			if (Mod.bQualified) Result += Mod.Magnitude;
		}
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::Override)])
		{
			if (Mod.bQualified) Result = Mod.Magnitude;
		}

		return Result;
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestSourcePayload
{
	GENERATED_BODY()

	UPROPERTY()
	float DamageMultiplier = 1.f;
};

USTRUCT(meta = (Hidden))
struct FArcTestSourceDataExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcAttributeRef TargetAttribute;
	float BaseValue = 0.f;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers
	) const override
	{
		float Multiplier = 1.f;
		const FArcTestSourcePayload* Payload = Context.GetOwningSpec().Context.SourceData.GetPtr<FArcTestSourcePayload>();
		if (Payload)
		{
			Multiplier = Payload->DamageMultiplier;
		}

		FArcEffectModifierOutput Output;
		Output.Attribute = TargetAttribute;
		Output.Operation = EArcModifierOp::Add;
		Output.Magnitude = BaseValue * Multiplier * Context.GetStackCount();
		OutModifiers.Add(Output);
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestCaptureExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcTestCaptureExecution()
	{
		FArcAttributeCaptureDef SourceCap;
		SourceCap.Attribute = FArcAttributeSet_TestStats::GetAttackPowerAttribute();
		SourceCap.CaptureSource = EArcCaptureSource::Source;
		SourceCap.bSnapshot = true;
		Captures.Add(SourceCap);

		FArcAttributeCaptureDef TargetCap;
		TargetCap.Attribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		TargetCap.CaptureSource = EArcCaptureSource::Target;
		TargetCap.bSnapshot = true;
		Captures.Add(TargetCap);
	}

	UPROPERTY()
	FArcAttributeRef OutputAttribute;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers) const override
	{
		FArcEvaluateParameters EvalParams;
		if (Context.EntityManager && Context.EntityManager->IsEntityValid(Context.GetSourceEntity()))
		{
			FMassEntityView SourceView(*Context.EntityManager, Context.GetSourceEntity());
			FArcOwnedTagsFragment* SourceTags = SourceView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
			if (SourceTags)
			{
				EvalParams.SourceTags = SourceTags->Tags.GetTagContainer();
			}
		}
		if (Context.EntityManager && Context.EntityManager->IsEntityValid(Context.TargetEntity))
		{
			FMassEntityView TargetView(*Context.EntityManager, Context.TargetEntity);
			FArcOwnedTagsFragment* TargetTags = TargetView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
			if (TargetTags)
			{
				EvalParams.TargetTags = TargetTags->Tags.GetTagContainer();
			}
		}

		float SourceVal = 0.f;
		Context.AttemptCalculateCapturedAttributeMagnitude(Captures[0], EvalParams, SourceVal);

		float TargetVal = 0.f;
		Context.AttemptCalculateCapturedAttributeMagnitude(Captures[1], EvalParams, TargetVal);

		float Damage = FMath::Max(0.f, SourceVal - TargetVal);

		FArcEffectModifierOutput Output;
		Output.Attribute = OutputAttribute;
		Output.Operation = EArcModifierOp::Add;
		Output.Magnitude = -Damage;
		OutModifiers.Add(Output);
	}
};

USTRUCT(meta = (Hidden, ToolTip = "Test-only handler: in PostChange, sets Armor.BaseValue to half the new Health value.\nAssign to FArcAttributeSet_TestStats::Health. Target: FArcAttributeSet_TestStats; accesses Armor through the same OwningFragmentMemory.\nSource: no required attribute sets; target means the entity that owns the attribute."))
struct FArcAttributeHandler_Test_ArmorFromHealth : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override
	{
		FArcAttributeSet_TestStats* Stats = static_cast<FArcAttributeSet_TestStats*>(static_cast<void*>(Context.OwningFragmentMemory));
		if (Stats)
		{
			Stats->Armor.BaseValue = NewValue * 0.5f;
		}
	}
};

USTRUCT(meta = (Hidden, ToolTip = "Test-only handler: in Initialize and PostChange, sets MaxEnergy.BaseValue to twice the new Intelligence value.\nAssign to FArcAttributeSet_TestStats::Intelligence. Target: FArcAttributeSet_TestStats and FArcAttributeSet_TestDerivedStats; skips the write if the latter is missing.\nSource: no required attribute sets; target means the entity that owns the attribute."))
struct FArcAttributeHandler_Test_EnergyFromIntelligence : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual void Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const override
	{
		FMassEntityView EntityView(Context.EntityManager, Context.Entity);
		FArcAttributeSet_TestDerivedStats* Derived = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();
		if (Derived)
		{
			Derived->MaxEnergy.BaseValue = NewValue * 2.f;
		}
	}

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override
	{
		FMassEntityView EntityView(Context.EntityManager, Context.Entity);
		FArcAttributeSet_TestDerivedStats* Derived = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();
		if (Derived)
		{
			Derived->MaxEnergy.BaseValue = NewValue * 2.f;
		}
	}
};

USTRUCT(meta = (Hidden, ToolTip = "Test-only handler: in PostExecute, subtracts nonzero Damage from Health without clamping and resets Damage to zero, even if the health set is missing.\nAssign to FArcAttributeSet_TestDerivedStats::Damage. Target: FArcAttributeSet_TestDerivedStats and FArcAttributeSet_TestStats to modify Health.\nSource: no required attribute sets; target means the entity that owns the attribute."))
struct FArcAttributeHandler_Test_DamageToHealth : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override
	{
		if (NewValue == 0.f)
		{
			return;
		}
		FMassEntityView EntityView(Context.EntityManager, Context.Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		if (Stats)
		{
			Stats->InitHealth(Stats->GetHealth() - NewValue);
		}
		Attribute.InitValue(0.f);
	}
};

USTRUCT(meta = (Hidden, ToolTip = "Test-only handler: counts Initialize, PostChange, and PostExecute separately without changing attributes.\nCan be assigned to any attribute under test. Target: that attribute's set; no additional sets required.\nSource: no required attribute sets; target means the entity that owns the attribute."))
struct FArcAttributeHandler_Test_PostExecuteTracker : public FArcAttributeHandler
{
	GENERATED_BODY()

	mutable int32 InitializeCount = 0;
	mutable int32 PostChangeCount = 0;
	mutable int32 PostExecuteCount = 0;

	virtual void Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const override
	{
		InitializeCount++;
	}

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override
	{
		PostChangeCount++;
	}

	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override
	{
		PostExecuteCount++;
	}
};

USTRUCT(meta = (Hidden, ToolTip = "Test-only handler: rejects every change in PreChange and counts any PostExecute calls.\nCan be assigned to any attribute being tested for change rejection. Target: that attribute's set; no additional sets required.\nSource: no required attribute sets; target means the entity that owns the attribute."))
struct FArcAttributeHandler_Test_Rejecting : public FArcAttributeHandler
{
	GENERATED_BODY()

	mutable int32 PostExecuteCount = 0;

	virtual bool PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue) const override
	{
		return false;
	}

	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override
	{
		PostExecuteCount++;
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestDamageCalcExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcTestDamageCalcExecution()
	{
		FArcAttributeCaptureDef SourceAttack;
		SourceAttack.Attribute = FArcAttributeSet_TestStats::GetAttackPowerAttribute();
		SourceAttack.CaptureSource = EArcCaptureSource::Source;
		SourceAttack.bSnapshot = false;
		Captures.Add(SourceAttack);

		FArcAttributeCaptureDef TargetArmor;
		TargetArmor.Attribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		TargetArmor.CaptureSource = EArcCaptureSource::Target;
		TargetArmor.bSnapshot = false;
		Captures.Add(TargetArmor);
	}

	UPROPERTY()
	FArcAttributeRef OutputAttribute;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers) const override
	{
		FArcEvaluateParameters EvalParams;
		if (Context.EntityManager && Context.EntityManager->IsEntityValid(Context.GetSourceEntity()))
		{
			FMassEntityView SourceView(*Context.EntityManager, Context.GetSourceEntity());
			FArcOwnedTagsFragment* SourceTags = SourceView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
			if (SourceTags)
			{
				EvalParams.SourceTags = SourceTags->Tags.GetTagContainer();
			}
		}
		if (Context.EntityManager && Context.EntityManager->IsEntityValid(Context.TargetEntity))
		{
			FMassEntityView TargetView(*Context.EntityManager, Context.TargetEntity);
			FArcOwnedTagsFragment* TargetTags = TargetView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
			if (TargetTags)
			{
				EvalParams.TargetTags = TargetTags->Tags.GetTagContainer();
			}
		}

		float AttackPower = 0.f;
		Context.AttemptCalculateCapturedAttributeMagnitude(Captures[0], EvalParams, AttackPower);

		float Armor = 0.f;
		Context.AttemptCalculateCapturedAttributeMagnitude(Captures[1], EvalParams, Armor);

		float Damage = FMath::Max(0.f, AttackPower - Armor);

		FArcEffectModifierOutput Output;
		Output.Attribute = OutputAttribute;
		Output.Operation = EArcModifierOp::Add;
		Output.Magnitude = Damage;
		OutModifiers.Add(Output);
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestInstantTaskInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta = (Hidden))
struct FArcTestInstantTask : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcTestInstantTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		return EStateTreeRunStatus::Succeeded;
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestExitFlagTaskInstanceData
{
	GENERATED_BODY()
};

/** Long-running task that records ExitState calls in a test-global counter — used to verify
 *  that cancel/remove paths Stop() the tree (running ExitState) instead of just freeing
 *  instance data. Reset ExitStateCount in each test before use. */
USTRUCT(meta = (Hidden))
struct FArcTestExitFlagTask : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcTestExitFlagTaskInstanceData;

	static int32 ExitStateCount;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		return EStateTreeRunStatus::Running;
	}
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		++ExitStateCount;
	}
};

USTRUCT(meta = (Hidden))
struct FArcAttributeSet_TestResource : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Energy = { 0.f, 0.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxEnergy = { 0.f, 0.f };

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_TestResource, Energy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_TestResource, MaxEnergy)
};

template<>
struct TMassFragmentTraits<FArcAttributeSet_TestResource> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// Bound to a "source" stat (e.g. Intelligence). On Initialize (seed time), fills both
// MaxEnergy and Energy to NewValue * 2 — entity starts at full. On PostChange (runtime),
// clamps Energy down to the new max without raising it, matching the fill-vs-clamp split.
USTRUCT(meta = (Hidden, ToolTip = "Test-only handler: Initialize sets MaxEnergy and fills Energy to twice the driving attribute's value; PostChange updates base MaxEnergy and only lowers current Energy that exceeds the maximum.\nTypically assign to FArcAttributeSet_TestStats::Intelligence. Target: the driving attribute's set and FArcAttributeSet_TestResource; skips the write if the resource set is missing.\nSource: no required attribute sets; target means the entity that owns the attribute."))
struct FArcAttributeHandler_Test_ResourceMaxFiller : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual void Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const override
	{
		FMassEntityView EntityView(Context.EntityManager, Context.Entity);
		FArcAttributeSet_TestResource* EnergyFrag = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestResource>();
		if (EnergyFrag)
		{
			const float NewMax = NewValue * 2.f;
			EnergyFrag->MaxEnergy.InitValue(NewMax);
			EnergyFrag->Energy.InitValue(NewMax);
		}
	}

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override
	{
		FMassEntityView EntityView(Context.EntityManager, Context.Entity);
		FArcAttributeSet_TestResource* EnergyFrag = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestResource>();
		if (EnergyFrag)
		{
			const float NewMax = NewValue * 2.f;
			EnergyFrag->MaxEnergy.SetBaseValue(NewMax);
			EnergyFrag->Energy.CurrentValue = FMath::Min(EnergyFrag->Energy.CurrentValue, NewMax);
		}
	}
};

// Counting effect task — member counter incremented each OnExecute call.
// Tasks are stored as FInstancedStruct copies per FArcActiveEffect, so each
// instance-stacked effect has its own independent counter.
USTRUCT(meta = (Hidden))
struct FArcEffectTask_Test_ExecuteCounter : public FArcEffectTask
{
	GENERATED_BODY()

	// mutable so the const-propagating FInstancedStruct path can still increment.
	mutable int32 ExecuteCount = 0;

	virtual void OnExecute(FArcActiveEffect& OwningEffect) override
	{
		ExecuteCount++;
	}
};

namespace ArcMassAbilitiesTestHelpers
{
	FMassEntityHandle CreateAbilityEntity(FMassEntityManager& EntityManager, float HealthBase = 100.f, float ArmorBase = 0.f);

	// Routes a heterogeneous FInstancedStruct list to UE::Mass::FEntityBuilder, which dispatches
	// regular vs sparse fragments correctly at Commit time. Use this wherever a test would have
	// called EntityManager.CreateEntity(FragmentInstances) with a list that contains any sparse
	// fragments (FArcMassAttributesBase subclasses, FArcOwnedTagsFragment,
	// FArcPendingAttributeOpsFragment, FArcImmunityFragment, etc.) — the raw CreateEntity overload
	// silently drops sparse fragments because they're not part of archetype composition.
	FMassEntityHandle CreateEntityFromInstances(FMassEntityManager& EntityManager, TConstArrayView<FInstancedStruct> Instances);

	inline FArcEffectExecutionEntry MakeExecutionEntry(FInstancedStruct&& Execution)
	{
		FArcEffectExecutionEntry Entry;
		Entry.CustomExecution = MoveTemp(Execution);
		return Entry;
	}
}
