// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectDurationCalculation.h"
#include "Effects/ArcEffectTagSource.h"
#include "ArcMassAbilitiesLogs.h"
#include "Effects/ArcExtraCapture.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcMagnitudeCalculation.h"
#include "Effects/ArcEffectModifier.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcEffectFunctions.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Attributes/ArcAggregator.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "UObject/GCObject.h"

class FArcEffectSpecReferenceOwner final : public FGCObject
{
public:
	explicit FArcEffectSpecReferenceOwner(FArcEffectSpec& InSpec)
		: Spec(&InSpec)
	{
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddPropertyReferencesWithStructARO(FArcEffectSpec::StaticStruct(), Spec);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("FArcEffectSpecReferenceOwner");
	}

private:
	FArcEffectSpec* Spec;
};

FArcEffectSpec::FArcEffectSpec()
	: ReferenceOwner(MakeShared<FArcEffectSpecReferenceOwner>(*this))
{
}

FArcEffectSpec::FArcEffectSpec(const FArcEffectSpec& Other)
	: FArcEffectSpec()
{
	*this = Other;
}

FArcEffectSpec::FArcEffectSpec(FArcEffectSpec&& Other)
	: FArcEffectSpec()
{
	*this = MoveTemp(Other);
}

FArcEffectSpec& FArcEffectSpec::operator=(const FArcEffectSpec& Other)
{
	if (this != &Other)
	{
		Definition = Other.Definition;
		Context = Other.Context;
		ResolvedMagnitudes = Other.ResolvedMagnitudes;
		SetByCallerMagnitudes = Other.SetByCallerMagnitudes;
		Duration = Other.Duration;
		CapturedSourceTags = Other.CapturedSourceTags;
		CapturedTargetTags = Other.CapturedTargetTags;
		ExecutionStates = Other.ExecutionStates;
		CapturedAttributes = Other.CapturedAttributes;
		CapturedExtras = Other.CapturedExtras;
		ResolvedScopedModifiers = Other.ResolvedScopedModifiers;
	}

	return *this;
}

FArcEffectSpec& FArcEffectSpec::operator=(FArcEffectSpec&& Other)
{
	if (this != &Other)
	{
		Definition = MoveTemp(Other.Definition);
		Context = MoveTemp(Other.Context);
		ResolvedMagnitudes = MoveTemp(Other.ResolvedMagnitudes);
		SetByCallerMagnitudes = MoveTemp(Other.SetByCallerMagnitudes);
		Duration = Other.Duration;
		CapturedSourceTags = MoveTemp(Other.CapturedSourceTags);
		CapturedTargetTags = MoveTemp(Other.CapturedTargetTags);
		ExecutionStates = MoveTemp(Other.ExecutionStates);
		CapturedAttributes = MoveTemp(Other.CapturedAttributes);
		CapturedExtras = MoveTemp(Other.CapturedExtras);
		ResolvedScopedModifiers = MoveTemp(Other.ResolvedScopedModifiers);
	}

	return *this;
}

namespace ArcEffectSpecCapture
{

FArcCapturedAttribute CaptureAttribute(FMassEntityManager& EntityManager, const FArcAttributeCaptureDef& CapDef,
                                       FMassEntityHandle ResolvedEntity, const FArcEvaluateParameters& EvalParams)
{
	FArcCapturedAttribute Captured;
	Captured.Definition = CapDef;
	Captured.bHasSnapshot = false;

	if (!CapDef.bSnapshot || !EntityManager.IsEntityValid(ResolvedEntity) || !CapDef.Attribute.IsValid())
	{
		return Captured;
	}

	FProperty* Prop = CapDef.Attribute.GetCachedProperty();
	UScriptStruct* FragStruct = CapDef.Attribute.FragmentType;
	if (!Prop || !FragStruct)
	{
		return Captured;
	}

	const uint8* FragMem = EntityManager.GetSparseElementDataForEntity(FragStruct, ResolvedEntity).GetMemory();
	if (!FragMem)
	{
		ARCMA_VLOG(EntityManager.GetWorld(), Warning,
			TEXT("[Effect] Attribute capture failed attr=%s::%s entity=%s fragment missing, using 0"),
			*FragStruct->GetName(),
			*CapDef.Attribute.PropertyName.ToString(),
			*ResolvedEntity.DebugGetDescription());
		return Captured;
	}

	const FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragMem);
	const float BaseValue = Attr->BaseValue;

	FMassEntityView ResolvedView(EntityManager, ResolvedEntity);
	FArcAggregatorFragment* AggFrag = ResolvedView.GetFragmentDataPtr<FArcAggregatorFragment>();
	FArcAggregator* Agg = AggFrag ? AggFrag->FindAggregator(CapDef.Attribute) : nullptr;
	if (Agg)
	{
		const FArcOwnedTagsFragment* OwnedTags = ResolvedView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		const FArcAggregationContext AggContext{ResolvedEntity,
			OwnedTags ? OwnedTags->GetTags().GetTagContainer() : FGameplayTagContainer::EmptyContainer,
			EntityManager, BaseValue};
		Captured.SnapshotValue = Agg->EvaluateWithTags(AggContext, EvalParams.SourceTags, EvalParams.TargetTags);
	}
	else
	{
		Captured.SnapshotValue = BaseValue;
	}

	Captured.bHasSnapshot = true;
	ARCMA_VLOG(EntityManager.GetWorld(), Verbose,
		TEXT("[Effect] Captured attr=%s::%s value=%.4f aggregated=%s entity=%s"),
		*FragStruct->GetName(),
		*CapDef.Attribute.PropertyName.ToString(),
		Captured.SnapshotValue,
		Agg ? TEXT("yes") : TEXT("no"),
		*ResolvedEntity.DebugGetDescription());
	return Captured;
}

FArcCapturedExtra CaptureExtra(const FArcEffectContext& Context, FMassEntityManager& EntityManager,
                               const FArcExtraCaptureDef& Def, const FArcEvaluateParameters& EvalParams)
{
	FArcCapturedExtra Slot;
	Slot.Definition = Def;

	const FArcExtraCaptureSource* Src = Def.Source.GetPtr<FArcExtraCaptureSource>();
	if (!Def.bSnapshot || !Src)
	{
		return Slot;
	}

	float Out = 0.f;
	Slot.bHasValue = Src->ResolveValue(Context, EntityManager, EvalParams, Out);
	if (Slot.bHasValue)
	{
		Slot.SnapshotValue = Out;
	}
	return Slot;
}

}

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
			Spec.ResolvedMagnitudes[Idx] = ArcEffects::ResolveScalableMagnitude(Mod.Magnitude, Mod.RangeResolution);
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
				UE_LOGF(LogArcMassAbilities, Warning,"SetByCaller tag '%ls' not found in spec, resolving to 0",
					*Mod.SetByCallerTag.ToString());
				UWorld* World = Spec.Context.EntityManager ? Spec.Context.EntityManager->GetWorld() : nullptr;
				ARCMA_VLOG(World, Warning,
					TEXT("[Effect] SetByCaller miss tag=%s effect=%s"),
					*Mod.SetByCallerTag.ToString(),
					Spec.Definition ? *Spec.Definition->GetName() : TEXT("<null>"));
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

		if (Exec->ScopedModifiers.IsEmpty())
		{
			continue;
		}

		TArray<FArcAttributeCaptureDef> ExecCaptures;
		TArray<FArcExtraCaptureDef> ExecExtras;
		Exec->GatherCaptures(ExecCaptures, ExecExtras);

		for (const FArcScopedModifier& ScopedMod : Exec->ScopedModifiers)
		{
			const FArcAttributeCaptureDef* Match = ExecCaptures.FindByPredicate(
				[&ScopedMod](const FArcAttributeCaptureDef& D) { return D == ScopedMod.CaptureDef; });
			if (!Match)
			{
				UE_LOGF(LogArcMassAbilities, Warning,"Scoped modifier capture def (%ls.%ls) not found among %d execution captures, skipping",
					*GetNameSafe(ScopedMod.CaptureDef.Attribute.FragmentType),
					*ScopedMod.CaptureDef.Attribute.PropertyName.ToString(), ExecCaptures.Num());
				continue;
			}

			float ResolvedMag = 0.f;
			switch (ScopedMod.Type)
			{
			case EArcModifierType::Simple:
				ResolvedMag = ArcEffects::ResolveScalableMagnitude(ScopedMod.Magnitude, ScopedMod.RangeResolution);
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
					UE_LOGF(LogArcMassAbilities, Warning,"Scoped modifier SetByCaller tag '%ls' not found, resolving to 0",
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
			Resolved.CaptureDef = *Match;
			Resolved.Operation = ScopedMod.Operation;
			Resolved.Magnitude = ResolvedMag;
			Resolved.SourceTagRequirements = ScopedMod.SourceTagRequirements;
			Resolved.TargetTagRequirements = ScopedMod.TargetTagRequirements;
			if (ScopedMod.Type == EArcModifierType::SetByCaller)
			{
				Resolved.SetByCallerTag = ScopedMod.SetByCallerTag;
			}
			Spec.AddResolvedScopedModifier(Resolved);
		}
	}
}

} // anonymous namespace

void ResolveScopedModifiers(FArcEffectSpec& Spec)
{
	ResolveScopedModifiersInternal(Spec);
}

void CaptureExtras(FArcEffectSpec& Spec, FMassEntityManager& EntityManager)
{
	if (!Spec.Definition)
	{
		return;
	}

	const FArcEffectCaptureLayout& Layout = Spec.Definition->GetCaptureLayout();
	const FArcEvaluateParameters EvalParams = Spec.MakeEvaluateParameters();

	for (const FArcExtraCaptureDef& Def : Layout.UniqueExtras)
	{
		Spec.AddCapturedExtra(ArcEffectSpecCapture::CaptureExtra(Spec.Context, EntityManager, Def, EvalParams));
	}
}

void CaptureAttributes(FArcEffectSpec& Spec, FMassEntityManager& EntityManager,
                        FMassEntityHandle Source, FMassEntityHandle Target)
{
	if (!Spec.Definition)
	{
		return;
	}

	const FArcEffectCaptureLayout& Layout = Spec.Definition->GetCaptureLayout();
	const FArcEvaluateParameters EvalParams = Spec.MakeEvaluateParameters();

	Spec.ReserveCapturedAttributes(Layout.UniqueCaptures.Num());
	for (const FArcAttributeCaptureDef& CapDef : Layout.UniqueCaptures)
	{
		const FMassEntityHandle ResolvedEntity = (CapDef.CaptureSource == EArcCaptureSource::Source) ? Source : Target;
		Spec.AddCapturedAttribute(ArcEffectSpecCapture::CaptureAttribute(EntityManager, CapDef, ResolvedEntity, EvalParams));
	}
}

FSharedStruct MakeSpec(UArcEffectDefinition* Definition, const FArcEffectContext& Context)
{
	FArcEffectApplyRequest Request;
	Request.Context = Context;
	Request.Definition = Definition;
	return MakeSpec(Request);
}

FSharedStruct MakeSpec(const FArcEffectApplyRequest& Request)
{
	UArcEffectDefinition* Definition = Request.Definition;

	FSharedStruct SharedSpec = FSharedStruct::Make<FArcEffectSpec>();
	FArcEffectSpec& Spec = SharedSpec.Get<FArcEffectSpec>();
	Spec.Definition = Definition;
	Spec.Context = Request.Context;
	Spec.Context.EffectDefinition = Definition;
	Spec.SetByCallerMagnitudes = Request.SetByCallerMagnitudes;

	if (!Definition)
	{
		return SharedSpec;
	}

	ArcEffectTagSources::GatherTags(Spec.Context, Spec.CapturedSourceTags, Spec.CapturedTargetTags);

	if (FMassEntityManager* EntityManager = Spec.Context.EntityManager)
	{
		CaptureAttributes(Spec, *EntityManager, Spec.Context.SourceEntity, Spec.Context.TargetEntity);
		CaptureExtras(Spec, *EntityManager);
	}

	ResolveScopedModifiersInternal(Spec);
	ResolveSpecMagnitudes(Spec);

	const float BaseDuration = Request.DurationOverride.Get(Definition->StackingPolicy.Duration);
	Spec.Duration = BaseDuration;
	if (const FArcEffectDurationCalculation* DurationCalc = Definition->DurationCalculation.GetPtr<FArcEffectDurationCalculation>())
	{
		FArcEffectExecutionContext DurationContext;
		DurationContext.OwningSpec = &Spec;
		DurationContext.EntityManager = Spec.Context.EntityManager;
		DurationContext.TargetEntity = Spec.Context.TargetEntity;
		Spec.Duration = DurationCalc->CalculateDuration(DurationContext, BaseDuration);
	}

	return SharedSpec;
}

void RebindSpecTarget(FArcEffectSpec& Spec, FMassEntityManager& EntityManager, FMassEntityHandle Target)
{
	Spec.Context.EntityManager = &EntityManager;
	Spec.Context.TargetEntity = Target;
	Spec.Context.EffectDefinition = Spec.Definition;
	Spec.Context.StackCount = FMath::Max(1, Spec.Context.StackCount);

	FGameplayTagContainer DiscardedSourceTags;
	FGameplayTagContainer TargetTags;
	ArcEffectTagSources::GatherTags(Spec.Context, DiscardedSourceTags, TargetTags);
	Spec.CapturedTargetTags = MoveTemp(TargetTags);

	if (!Spec.Definition)
	{
		return;
	}

	const FArcEffectCaptureLayout& Layout = Spec.Definition->GetCaptureLayout();
	const FArcEvaluateParameters EvalParams = Spec.MakeEvaluateParameters();

	for (const FArcAttributeCaptureDef& CapDef : Layout.UniqueCaptures)
	{
		if (CapDef.CaptureSource != EArcCaptureSource::Target)
		{
			continue;
		}

		if (FArcCapturedAttribute* Captured = Spec.FindMutableCapturedAttribute(CapDef))
		{
			*Captured = ArcEffectSpecCapture::CaptureAttribute(EntityManager, CapDef, Target, EvalParams);
		}
	}

	for (const FArcExtraCaptureDef& Def : Layout.UniqueExtras)
	{
		const FArcExtraCaptureSource* Src = Def.Source.GetPtr<FArcExtraCaptureSource>();
		if (!Src || Src->GetCaptureSide() != EArcCaptureSource::Target)
		{
			continue;
		}

		if (FArcCapturedExtra* Captured = Spec.FindMutableCapturedExtra(Def))
		{
			*Captured = ArcEffectSpecCapture::CaptureExtra(Spec.Context, EntityManager, Def, EvalParams);
		}
	}
}

} // namespace ArcEffects
