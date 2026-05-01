// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectSpec.h"
#include "Attributes/ArcAggregator.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

const FArcEffectSpec& FArcEffectExecutionContext::GetOwningSpec() const
{
	check(OwningSpec);
	return *OwningSpec;
}

FMassEntityHandle FArcEffectExecutionContext::GetSourceEntity() const
{
	check(OwningSpec);
	return OwningSpec->Context.SourceEntity;
}

int32 FArcEffectExecutionContext::GetStackCount() const
{
	check(OwningSpec);
	return OwningSpec->Context.StackCount;
}

FArcAttribute FArcEffectExecutionContext::GetAttribute(FMassEntityHandle Entity, const FArcAttributeRef& Ref) const
{
	if (!EntityManager || !Ref.IsValid())
	{
		return FArcAttribute();
	}

	const FMassEntityView EntityView(*EntityManager, Entity);
	UScriptStruct* FragStruct = Ref.FragmentType;
	FStructView FragmentView = EntityView.GetFragmentDataStruct(FragStruct);
	if (!FragmentView.IsValid())
	{
		return FArcAttribute();
	}

	FProperty* Prop = Ref.GetCachedProperty();
	if (!Prop)
	{
		return FArcAttribute();
	}

	const FArcAttribute* AttributePtr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragmentView.GetMemory());
	return *AttributePtr;
}

bool FArcEffectExecutionContext::AttemptCalculateCapturedAttributeMagnitude(
	const FArcAttributeCaptureDef& CaptureDef,
	const FArcEvaluateParameters& EvalParams,
	float& OutMagnitude) const
{
	check(OwningSpec);

	bool bFound = false;
	float BaseValue = 0.f;

	for (const FArcCapturedAttribute& Captured : OwningSpec->CapturedAttributes)
	{
		if (!(Captured.Definition == CaptureDef))
		{
			continue;
		}

		if (Captured.bHasSnapshot)
		{
			BaseValue = Captured.SnapshotValue;
			bFound = true;
			break;
		}

		FMassEntityHandle Entity = (CaptureDef.CaptureSource == EArcCaptureSource::Source)
			? OwningSpec->Context.SourceEntity
			: TargetEntity;

		if (!EntityManager || !EntityManager->IsEntityValid(Entity))
		{
			return false;
		}

		FMassEntityView EntityView(*EntityManager, Entity);

		FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
		if (AggFragment)
		{
			FArcAggregator* Aggregator = AggFragment->FindAggregator(CaptureDef.Attribute);
			if (Aggregator)
			{
				FArcAttribute BaseAttr = GetAttribute(Entity, CaptureDef.Attribute);
				BaseValue = Aggregator->EvaluateWithTags(
					BaseAttr.BaseValue,
					EvalParams.SourceTags,
					EvalParams.TargetTags);
				bFound = true;
				break;
			}
		}

		FArcAttribute Attr = GetAttribute(Entity, CaptureDef.Attribute);
		BaseValue = Attr.BaseValue;
		bFound = true;
		break;
	}

	if (!bFound)
	{
		return false;
	}

	float SumAdd = 0.f;
	float SumMulAdd = 0.f;
	float SumDivAdd = 0.f;
	float ProdMulCompound = 1.f;
	float SumAddFinal = 0.f;
	float OverrideValue = 0.f;
	bool bHasOverride = false;

	for (const FArcResolvedScopedMod& Mod : OwningSpec->ResolvedScopedModifiers)
	{
		if (!(Mod.CaptureDef == CaptureDef))
		{
			continue;
		}

		switch (Mod.Operation)
		{
		case EArcModifierOp::Add:
			SumAdd += Mod.Magnitude;
			break;
		case EArcModifierOp::MultiplyAdditive:
			SumMulAdd += Mod.Magnitude;
			break;
		case EArcModifierOp::DivideAdditive:
			SumDivAdd += Mod.Magnitude;
			break;
		case EArcModifierOp::MultiplyCompound:
			ProdMulCompound *= Mod.Magnitude;
			break;
		case EArcModifierOp::AddFinal:
			SumAddFinal += Mod.Magnitude;
			break;
		case EArcModifierOp::Override:
			OverrideValue = Mod.Magnitude;
			bHasOverride = true;
			break;
		default:
			break;
		}
	}

	float Result = BaseValue;
	Result += SumAdd;
	Result *= (1.f + SumMulAdd);
	float Divisor = 1.f + SumDivAdd;
	if (Divisor != 0.f)
	{
		Result /= Divisor;
	}
	Result *= ProdMulCompound;
	Result += SumAddFinal;
	if (bHasOverride)
	{
		Result = OverrideValue;
	}

	OutMagnitude = Result;
	return true;
}
