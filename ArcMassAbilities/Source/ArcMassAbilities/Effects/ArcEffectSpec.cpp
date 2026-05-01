// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcMagnitudeCalculation.h"
#include "Effects/ArcEffectModifier.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcEffectExecution.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Attributes/ArcAggregator.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

namespace ArcEffects
{

namespace
{

void ResolveSpecMagnitudes(FArcEffectSpec& Spec)
{
	if (!Spec.Definition)
	{
		return;
	}

	const TArray<FArcEffectModifier>& Modifiers = Spec.Definition->Modifiers;
	Spec.ResolvedMagnitudes.SetNum(Modifiers.Num());

	for (int32 Idx = 0; Idx < Modifiers.Num(); ++Idx)
	{
		const FArcEffectModifier& Mod = Modifiers[Idx];

		switch (Mod.Type)
		{
		case EArcModifierType::Simple:
			Spec.ResolvedMagnitudes[Idx] = Mod.Magnitude.GetValue();
			break;

		case EArcModifierType::SetByCaller:
		{
			const float* Found = Spec.SetByCallerMagnitudes.Find(Mod.SetByCallerTag);
			if (Found)
			{
				Spec.ResolvedMagnitudes[Idx] = *Found;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("SetByCaller tag '%s' not found in spec, resolving to 0"),
					*Mod.SetByCallerTag.ToString());
				Spec.ResolvedMagnitudes[Idx] = 0.f;
			}
			break;
		}

		case EArcModifierType::Custom:
		{
			if (Mod.CustomMagnitude.IsValid())
			{
				const FArcMagnitudeCalculation* Calc = Mod.CustomMagnitude.GetPtr<FArcMagnitudeCalculation>();
				if (Calc)
				{
					Spec.ResolvedMagnitudes[Idx] = Calc->Calculate(Spec.Context);
				}
				else
				{
					Spec.ResolvedMagnitudes[Idx] = 0.f;
				}
			}
			else
			{
				Spec.ResolvedMagnitudes[Idx] = 0.f;
			}
			break;
		}
		}
	}
}

void ResolveScopedModifiersInternal(FArcEffectSpec& Spec)
{
	if (!Spec.Definition)
	{
		return;
	}

	for (const FArcEffectExecutionEntry& Entry : Spec.Definition->Executions)
	{
		if (!Entry.CustomExecution.IsValid())
		{
			continue;
		}
		const FArcEffectExecution* Exec = Entry.CustomExecution.GetPtr<FArcEffectExecution>();
		if (!Exec)
		{
			continue;
		}

		for (const FArcScopedModifier& ScopedMod : Exec->ScopedModifiers)
		{
			if (!Exec->Captures.IsValidIndex(ScopedMod.CaptureIndex))
			{
				UE_LOG(LogTemp, Warning, TEXT("Scoped modifier CaptureIndex %d out of range (Captures has %d entries), skipping"),
					ScopedMod.CaptureIndex, Exec->Captures.Num());
				continue;
			}

			if (ScopedMod.TagFilter.IsEmpty() == false)
			{
				bool bSourceMet = ScopedMod.TagFilter.RequirementsMet(Spec.CapturedSourceTags);
				if (!bSourceMet)
				{
					continue;
				}
			}

			float ResolvedMag = 0.f;
			switch (ScopedMod.Type)
			{
			case EArcModifierType::Simple:
				ResolvedMag = ScopedMod.Magnitude.GetValue();
				break;

			case EArcModifierType::SetByCaller:
			{
				const float* Found = Spec.SetByCallerMagnitudes.Find(ScopedMod.SetByCallerTag);
				if (Found)
				{
					ResolvedMag = *Found;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Scoped modifier SetByCaller tag '%s' not found, resolving to 0"),
						*ScopedMod.SetByCallerTag.ToString());
				}
				break;
			}

			case EArcModifierType::Custom:
			{
				if (ScopedMod.CustomMagnitude.IsValid())
				{
					const FArcMagnitudeCalculation* Calc = ScopedMod.CustomMagnitude.GetPtr<FArcMagnitudeCalculation>();
					if (Calc)
					{
						ResolvedMag = Calc->Calculate(Spec.Context);
					}
				}
				break;
			}
			}

			FArcResolvedScopedMod Resolved;
			Resolved.CaptureDef = Exec->Captures[ScopedMod.CaptureIndex];
			Resolved.Operation = ScopedMod.Operation;
			Resolved.Magnitude = ResolvedMag;
			Spec.ResolvedScopedModifiers.Add(Resolved);
		}
	}
}

} // anonymous namespace

void ResolveScopedModifiers(FArcEffectSpec& Spec)
{
	ResolveScopedModifiersInternal(Spec);
}

void CaptureTagsFromEntities(FArcEffectSpec& Spec, FMassEntityManager& EntityManager,
                              FMassEntityHandle Source, FMassEntityHandle Target)
{
	if (EntityManager.IsEntityValid(Source))
	{
		FMassEntityView SourceView(EntityManager, Source);
		const FArcOwnedTagsFragment* SourceTagsFrag = SourceView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
		if (SourceTagsFrag)
		{
			Spec.CapturedSourceTags.AppendTags(SourceTagsFrag->Tags.GetTagContainer());
		}
	}

	if (EntityManager.IsEntityValid(Target))
	{
		FMassEntityView TargetView(EntityManager, Target);
		const FArcOwnedTagsFragment* TargetTagsFrag = TargetView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
		if (TargetTagsFrag)
		{
			Spec.CapturedTargetTags.AppendTags(TargetTagsFrag->Tags.GetTagContainer());
		}
	}
}

void CaptureTagsFromDefinition(FArcEffectSpec& Spec, UArcEffectDefinition* Definition)
{
	if (!Definition)
	{
		return;
	}

	Spec.CapturedSourceTags.AppendTags(ArcEffectHelpers::GetEffectTags(Definition));

	if (Spec.Context.SourceAbility)
	{
		Spec.CapturedSourceTags.AppendTags(Spec.Context.SourceAbility->AbilityTags);
	}
}

void CaptureAttributes(FArcEffectSpec& Spec, FMassEntityManager& EntityManager,
                        FMassEntityHandle Source, FMassEntityHandle Target)
{
	if (!Spec.Definition)
	{
		return;
	}

	TArray<FArcAttributeCaptureDef> AllCaptureDefs;
	for (const FArcEffectExecutionEntry& Entry : Spec.Definition->Executions)
	{
		if (!Entry.CustomExecution.IsValid())
		{
			continue;
		}
		const FArcEffectExecution* Exec = Entry.CustomExecution.GetPtr<FArcEffectExecution>();
		if (!Exec)
		{
			continue;
		}
		for (const FArcAttributeCaptureDef& CapDef : Exec->Captures)
		{
			bool bAlreadyExists = false;
			for (const FArcAttributeCaptureDef& Existing : AllCaptureDefs)
			{
				if (Existing == CapDef)
				{
					bAlreadyExists = true;
					break;
				}
			}
			if (!bAlreadyExists)
			{
				AllCaptureDefs.Add(CapDef);
			}
		}
	}

	FArcEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags;
	EvalParams.TargetTags = Spec.CapturedTargetTags;

	Spec.CapturedAttributes.Reserve(AllCaptureDefs.Num());
	for (const FArcAttributeCaptureDef& CapDef : AllCaptureDefs)
	{
		FArcCapturedAttribute Captured;
		Captured.Definition = CapDef;

		if (!CapDef.bSnapshot)
		{
			Captured.bHasSnapshot = false;
			Spec.CapturedAttributes.Add(Captured);
			continue;
		}

		FMassEntityHandle ResolvedEntity = (CapDef.CaptureSource == EArcCaptureSource::Source) ? Source : Target;

		if (!EntityManager.IsEntityValid(ResolvedEntity) || !CapDef.Attribute.IsValid())
		{
			Captured.bHasSnapshot = false;
			Spec.CapturedAttributes.Add(Captured);
			continue;
		}

		FMassEntityView ResolvedView(EntityManager, ResolvedEntity);

		FProperty* Prop = CapDef.Attribute.GetCachedProperty();
		UScriptStruct* FragStruct = CapDef.Attribute.FragmentType;
		if (!Prop || !FragStruct)
		{
			Captured.bHasSnapshot = false;
			Spec.CapturedAttributes.Add(Captured);
			continue;
		}

		uint8* FragMem = ResolvedView.GetFragmentDataStruct(FragStruct).GetMemory();
		if (!FragMem)
		{
			Captured.bHasSnapshot = false;
			Spec.CapturedAttributes.Add(Captured);
			continue;
		}

		const FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragMem);
		float BaseValue = Attr->BaseValue;

		FArcAggregatorFragment* AggFrag = ResolvedView.GetFragmentDataPtr<FArcAggregatorFragment>();
		if (AggFrag)
		{
			FArcAggregator* Agg = AggFrag->FindAggregator(CapDef.Attribute);
			if (Agg)
			{
				Captured.SnapshotValue = Agg->EvaluateWithTags(BaseValue, EvalParams.SourceTags, EvalParams.TargetTags);
				Captured.bHasSnapshot = true;
				Spec.CapturedAttributes.Add(Captured);
				continue;
			}
		}

		Captured.SnapshotValue = BaseValue;
		Captured.bHasSnapshot = true;
		Spec.CapturedAttributes.Add(Captured);
	}
}

FSharedStruct MakeSpec(UArcEffectDefinition* Definition, const FArcEffectContext& Context)
{
	FSharedStruct SharedSpec = FSharedStruct::Make<FArcEffectSpec>();
	FArcEffectSpec& Spec = SharedSpec.Get<FArcEffectSpec>();
	Spec.Definition = Definition;
	Spec.Context = Context;

	if (Definition && Context.EntityManager)
	{
		CaptureTagsFromEntities(Spec, *Context.EntityManager, Context.SourceEntity, Context.TargetEntity);
		CaptureTagsFromDefinition(Spec, Definition);
		CaptureAttributes(Spec, *Context.EntityManager, Context.SourceEntity, Context.TargetEntity);
	}

	if (Definition)
	{
		ResolveScopedModifiersInternal(Spec);
		ResolveSpecMagnitudes(Spec);
	}

	return SharedSpec;
}

} // namespace ArcEffects
