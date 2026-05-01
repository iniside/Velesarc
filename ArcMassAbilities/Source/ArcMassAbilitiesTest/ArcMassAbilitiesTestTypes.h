// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcAttributeCapture.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Effects/ArcEffectDefinition.h"
#include "MassEntityView.h"
#include "MassEntityManager.h"
#include "ArcMassAbilitiesTestTypes.generated.h"

USTRUCT()
struct FArcTestStatsFragment : public FArcMassAttributesBase
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

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcTestStatsFragment, Health)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcTestStatsFragment, Armor)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcTestStatsFragment, Intelligence)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcTestStatsFragment, AttackPower)
};

template<>
struct TMassFragmentTraits<FArcTestStatsFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct FArcTestDerivedStatsFragment : public FArcMassAttributesBase
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
			R.FragmentType = FArcTestDerivedStatsFragment::StaticStruct();
			R.PropertyName = GET_MEMBER_NAME_CHECKED(FArcTestDerivedStatsFragment, MaxEnergy);
			return R;
		}();
		return Ref;
	}

	static FArcAttributeRef GetDamageAttribute()
	{
		static FArcAttributeRef Ref = []()
		{
			FArcAttributeRef R;
			R.FragmentType = FArcTestDerivedStatsFragment::StaticStruct();
			R.PropertyName = GET_MEMBER_NAME_CHECKED(FArcTestDerivedStatsFragment, Damage);
			return R;
		}();
		return Ref;
	}
};

template<>
struct TMassFragmentTraits<FArcTestDerivedStatsFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct FArcTestAddExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcAttributeRef TargetAttribute;
	float Value = 0.f;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers
	) override
	{
		FArcEffectModifierOutput Output;
		Output.Attribute = TargetAttribute;
		Output.Operation = EArcModifierOp::Add;
		Output.Magnitude = Value * Context.GetStackCount();
		OutModifiers.Add(Output);
	}
};

USTRUCT()
struct FArcTestOverrideExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcAttributeRef TargetAttribute;
	float Value = 0.f;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers
	) override
	{
		FArcEffectModifierOutput Output;
		Output.Attribute = TargetAttribute;
		Output.Operation = EArcModifierOp::Override;
		Output.Magnitude = Value;
		OutModifiers.Add(Output);
	}
};

USTRUCT()
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

USTRUCT()
struct FArcTestSourcePayload
{
	GENERATED_BODY()

	UPROPERTY()
	float DamageMultiplier = 1.f;
};

USTRUCT()
struct FArcTestSourceDataExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcAttributeRef TargetAttribute;
	float BaseValue = 0.f;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers
	) override
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

USTRUCT()
struct FArcTestCaptureExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcTestCaptureExecution()
	{
		FArcAttributeCaptureDef SourceCap;
		SourceCap.Attribute = FArcTestStatsFragment::GetAttackPowerAttribute();
		SourceCap.CaptureSource = EArcCaptureSource::Source;
		SourceCap.bSnapshot = true;
		Captures.Add(SourceCap);

		FArcAttributeCaptureDef TargetCap;
		TargetCap.Attribute = FArcTestStatsFragment::GetArmorAttribute();
		TargetCap.CaptureSource = EArcCaptureSource::Target;
		TargetCap.bSnapshot = true;
		Captures.Add(TargetCap);
	}

	UPROPERTY()
	FArcAttributeRef OutputAttribute;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers) override
	{
		FArcEvaluateParameters EvalParams;
		if (Context.EntityManager && Context.EntityManager->IsEntityValid(Context.GetSourceEntity()))
		{
			FMassEntityView SourceView(*Context.EntityManager, Context.GetSourceEntity());
			FArcOwnedTagsFragment* SourceTags = SourceView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
			if (SourceTags)
			{
				EvalParams.SourceTags = SourceTags->Tags.GetTagContainer();
			}
		}
		if (Context.EntityManager && Context.EntityManager->IsEntityValid(Context.TargetEntity))
		{
			FMassEntityView TargetView(*Context.EntityManager, Context.TargetEntity);
			FArcOwnedTagsFragment* TargetTags = TargetView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
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

USTRUCT()
struct FArcTestArmorFromHealthHandler : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) override
	{
		FArcTestStatsFragment* Stats = static_cast<FArcTestStatsFragment*>(static_cast<void*>(Context.OwningFragmentMemory));
		if (Stats)
		{
			Stats->Armor.BaseValue = NewValue * 0.5f;
		}
	}
};

USTRUCT()
struct FArcTestEnergyFromIntelligenceHandler : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) override
	{
		FMassEntityView EntityView(Context.EntityManager, Context.Entity);
		FArcTestDerivedStatsFragment* Derived = EntityView.GetFragmentDataPtr<FArcTestDerivedStatsFragment>();
		if (Derived)
		{
			Derived->MaxEnergy.BaseValue = NewValue * 2.f;
		}
	}
};

USTRUCT()
struct FArcTestDamageToHealthHandler : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) override
	{
		if (NewValue == 0.f)
		{
			return;
		}
		FMassEntityView EntityView(Context.EntityManager, Context.Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		if (Stats)
		{
			Stats->SetHealth(Stats->GetHealth() - NewValue);
		}
		Attribute.SetBaseValue(0.f);
	}
};

USTRUCT()
struct FArcTestPostExecuteTracker : public FArcAttributeHandler
{
	GENERATED_BODY()

	mutable int32 PostChangeCount = 0;
	mutable int32 PostExecuteCount = 0;

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) override
	{
		PostChangeCount++;
	}

	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) override
	{
		PostExecuteCount++;
	}
};

USTRUCT()
struct FArcTestRejectingHandler : public FArcAttributeHandler
{
	GENERATED_BODY()

	mutable int32 PostExecuteCount = 0;

	virtual bool PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue) override
	{
		return false;
	}

	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) override
	{
		PostExecuteCount++;
	}
};

USTRUCT()
struct FArcTestDamageCalcExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	FArcTestDamageCalcExecution()
	{
		FArcAttributeCaptureDef SourceAttack;
		SourceAttack.Attribute = FArcTestStatsFragment::GetAttackPowerAttribute();
		SourceAttack.CaptureSource = EArcCaptureSource::Source;
		SourceAttack.bSnapshot = false;
		Captures.Add(SourceAttack);

		FArcAttributeCaptureDef TargetArmor;
		TargetArmor.Attribute = FArcTestStatsFragment::GetArmorAttribute();
		TargetArmor.CaptureSource = EArcCaptureSource::Target;
		TargetArmor.bSnapshot = false;
		Captures.Add(TargetArmor);
	}

	UPROPERTY()
	FArcAttributeRef OutputAttribute;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers) override
	{
		FArcEvaluateParameters EvalParams;
		if (Context.EntityManager && Context.EntityManager->IsEntityValid(Context.GetSourceEntity()))
		{
			FMassEntityView SourceView(*Context.EntityManager, Context.GetSourceEntity());
			FArcOwnedTagsFragment* SourceTags = SourceView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
			if (SourceTags)
			{
				EvalParams.SourceTags = SourceTags->Tags.GetTagContainer();
			}
		}
		if (Context.EntityManager && Context.EntityManager->IsEntityValid(Context.TargetEntity))
		{
			FMassEntityView TargetView(*Context.EntityManager, Context.TargetEntity);
			FArcOwnedTagsFragment* TargetTags = TargetView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
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

USTRUCT()
struct FArcTestInstantTaskInstanceData
{
	GENERATED_BODY()
};

USTRUCT()
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

namespace ArcMassAbilitiesTestHelpers
{
	FMassEntityHandle CreateAbilityEntity(FMassEntityManager& EntityManager, float HealthBase = 100.f, float ArmorBase = 0.f);

	inline FArcEffectExecutionEntry MakeExecutionEntry(FInstancedStruct&& Execution)
	{
		FArcEffectExecutionEntry Entry;
		Entry.CustomExecution = MoveTemp(Execution);
		return Entry;
	}
}
