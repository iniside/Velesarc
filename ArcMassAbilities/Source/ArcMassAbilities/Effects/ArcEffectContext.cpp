// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcExtraCapture.h"
#include "Attributes/ArcAggregator.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

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

	UScriptStruct* FragStruct = Ref.FragmentType;
	FConstStructView FragmentView = EntityManager->GetSparseElementDataForEntity(FragStruct, Entity);
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

bool FArcEffectExecutionContext::ComputeCapturedMagnitude(
	const FArcCapturedAttribute& Captured,
	const FArcEvaluateParameters& EvalParams,
	float& OutMagnitude) const
{
	const FArcAttributeCaptureDef& CaptureDef = Captured.Definition;

	float BaseValue = 0.f;

	if (Captured.bHasSnapshot)
	{
		BaseValue = Captured.SnapshotValue;
	}
	else
	{
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
				const FArcOwnedTagsFragment* OwnedTags = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
				const FArcAggregationContext AggContext{Entity,
					OwnedTags ? OwnedTags->GetTags().GetTagContainer() : FGameplayTagContainer::EmptyContainer,
					*EntityManager, BaseAttr.BaseValue};
				BaseValue = Aggregator->EvaluateWithTags(AggContext, EvalParams.SourceTags, EvalParams.TargetTags);
			}
			else
			{
				FArcAttribute Attr = GetAttribute(Entity, CaptureDef.Attribute);
				BaseValue = Attr.BaseValue;
			}
		}
		else
		{
			FArcAttribute Attr = GetAttribute(Entity, CaptureDef.Attribute);
			BaseValue = Attr.BaseValue;
		}
	}

	float SumAdd = 0.f;
	float SumMulAdd = 0.f;
	float SumDivAdd = 0.f;
	float ProdMulCompound = 1.f;
	float SumAddFinal = 0.f;
	float OverrideValue = 0.f;
	bool bHasOverride = false;

	OwningSpec->ForEachScopedModifierForDef(CaptureDef, [&](const FArcResolvedScopedMod& Mod)
	{
		const bool bSourceMet = Mod.SourceTagRequirements.IsEmpty()
			|| Mod.SourceTagRequirements.RequirementsMet(EvalParams.SourceTags);
		const bool bTargetMet = Mod.TargetTagRequirements.IsEmpty()
			|| Mod.TargetTagRequirements.RequirementsMet(EvalParams.TargetTags);
		if (!bSourceMet || !bTargetMet)
		{
			return;
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
	});

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

bool FArcEffectExecutionContext::ResolveExtraAt(const FArcCapturedExtra& Entry, const FArcEvaluateParameters& EvalParams, float& OutValue) const
{
	if (Entry.Definition.bSnapshot)
	{
		if (!Entry.bHasValue)
		{
			return false;
		}
		OutValue = Entry.SnapshotValue;
		return true;
	}

	const FArcExtraCaptureSource* Src = Entry.Definition.Source.GetPtr<FArcExtraCaptureSource>();
	if (!Src)
	{
		return false;
	}
	return Src->ResolveValue(OwningSpec->Context, *EntityManager, EvalParams, OutValue);
}

bool FArcEffectExecutionContext::AttemptCalculateCapturedAttributeMagnitude(
	const FArcAttributeCaptureDef& CaptureDef,
	const FArcEvaluateParameters& EvalParams,
	float& OutMagnitude) const
{
	check(OwningSpec);
	const FArcCapturedAttribute* Captured = OwningSpec->FindCapturedAttribute(CaptureDef);
	if (!Captured) { return false; }
	return ComputeCapturedMagnitude(*Captured, EvalParams, OutMagnitude);
}

bool FArcEffectExecutionContext::AttemptResolveExtraCapture(
	const FArcExtraCaptureDef& Def, const FArcEvaluateParameters& EvalParams, float& OutValue) const
{
	if (!OwningSpec || !EntityManager) { return false; }
	const FArcCapturedExtra* Entry = OwningSpec->FindCapturedExtra(Def);
	if (!Entry) { return false; }
	return ResolveExtraAt(*Entry, EvalParams, OutValue);
}

