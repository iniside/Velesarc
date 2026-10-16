// Copyright Lukasz Baran. All Rights Reserved.

#include "Modifiers/ArcModifierFunctions.h"

#include "ArcMassAbilitiesLogs.h"
#include "Mass/EntityManager.h"
#include "Attributes/ArcAttribute.h"
#include "Modifiers/ArcDirectModifier.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"

namespace ArcModifiersPrivate
{

void SendRecalculateSignal(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
	}
}

void SendPendingOpsSignal(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
	}
}

} // namespace ArcModifiersPrivate

namespace ArcAttributes
{

FArcAttribute* ResolveAttribute(const FArcAttributeRef& Attribute, uint8* FragmentMemory)
{
	if (!FragmentMemory || !Attribute.IsValid())
	{
		return nullptr;
	}

	FProperty* Prop = Attribute.GetCachedProperty();
	if (!Prop)
	{
		return nullptr;
	}

	return Prop->ContainerPtrToValuePtr<FArcAttribute>(FragmentMemory);
}

FArcAttribute* ResolveAttribute(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcAttributeRef& Attribute)
{
	if (!EntityManager.IsEntityValid(Entity) || !Attribute.IsValid())
	{
		return nullptr;
	}

	FStructView FragView = EntityManager.GetMutableSparseElementDataForEntity(Attribute.FragmentType, Entity);
	uint8* Memory = FragView.GetMemory();

	return ResolveAttribute(Attribute, Memory);
}

void QueueSetValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
                   const FArcAttributeRef& Attribute, float NewValue)
{
	if (!EntityManager.IsEntityValid(Entity) || !Attribute.IsValid())
	{
		return;
	}

	FMassEntityView View(EntityManager, Entity);
	FArcPendingAttributeOpsFragment* Pending = View.GetSparseFragmentDataPtr<FArcPendingAttributeOpsFragment>();
	if (!Pending)
	{
		return;
	}

	FArcPendingAttributeOp& Op = Pending->PendingOps.AddDefaulted_GetRef();
	Op.Attribute = Attribute;
	Op.Operation = EArcModifierOp::Override;
	Op.Magnitude = NewValue;

	ArcModifiersPrivate::SendPendingOpsSignal(EntityManager, Entity);
}

// Not affected by the SetBaseValue-becomes-base-only change: QueueSetValue queues an Override
// pending op; the PendingAttributeOps processor writes BaseValue and drives CurrentValue through
// the full recalc + NotifyAttributeChanged path. FArcAttribute::SetBaseValue is never called here.
void SetValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
              const FArcAttributeRef& Attribute, float NewValue)
{
	QueueSetValue(EntityManager, Entity, Attribute, NewValue);
}

float GetValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
               const FArcAttributeRef& Attribute)
{
	FArcAttribute* Attr = ResolveAttribute(EntityManager, Entity, Attribute);
	return Attr ? Attr->GetCurrentValue() : 0.f;
}

float EvaluateAttributeWithTags(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const FArcAttributeRef& Attribute,
	float BaseValue,
	const FGameplayTagContainer& SourceTags,
	const FGameplayTagContainer& TargetTags)
{
	if (!EntityManager.IsEntityValid(Entity) || !Attribute.IsValid())
	{
		return BaseValue;
	}

	FMassEntityView View(EntityManager, Entity);
	const FArcAggregatorFragment* AggFrag = View.GetFragmentDataPtr<FArcAggregatorFragment>();
	if (!AggFrag)
	{
		return BaseValue;
	}

	return EvaluateAttributeWithTagsUnchecked(EntityManager, Entity, Attribute, BaseValue, SourceTags, TargetTags);
}

float EvaluateAttributeWithTagsUnchecked(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const FArcAttributeRef& Attribute,
	float BaseValue,
	const FGameplayTagContainer& SourceTags,
	const FGameplayTagContainer& TargetTags)
{
	FMassEntityView View(EntityManager, Entity);
	const FArcAggregatorFragment& AggFrag = *View.GetFragmentDataPtr<FArcAggregatorFragment>();
	const FArcAggregator* Agg = AggFrag.Aggregators.Find(Attribute);
	if (!Agg)
	{
		return BaseValue;
	}

	const FArcOwnedTagsFragment* OwnedTags = View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
	const FArcAggregationContext AggContext{Entity,
		OwnedTags ? OwnedTags->GetTags().GetTagContainer() : FGameplayTagContainer::EmptyContainer,
		EntityManager, BaseValue};
	return Agg->EvaluateWithTags(AggContext, SourceTags, TargetTags);
}

} // namespace ArcAttributes

namespace
{

void QueueOpInternal(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                     const FArcAttributeRef& Attribute, EArcModifierOp Op,
                     float Magnitude, EArcAttributeOpType OpType)
{
	if (!EntityManager.IsEntityValid(Target) || !Attribute.IsValid())
	{
		return;
	}

	FMassEntityView View(EntityManager, Target);
	FArcPendingAttributeOpsFragment* Pending = View.GetSparseFragmentDataPtr<FArcPendingAttributeOpsFragment>();
	if (!Pending)
	{
		return;
	}

	ARCMA_VLOG(EntityManager.GetWorld(), Log,
		TEXT("[Attr] Op queued attr=%s::%s op=%d magnitude=%.4f entity=%s"),
		Attribute.FragmentType ? *Attribute.FragmentType->GetName() : TEXT("<null>"),
		*Attribute.PropertyName.ToString(),
		static_cast<int32>(Op),
		Magnitude,
		*Target.DebugGetDescription());

	FArcPendingAttributeOp& PendingOp = Pending->PendingOps.AddDefaulted_GetRef();
	PendingOp.Attribute = Attribute;
	PendingOp.Operation = Op;
	PendingOp.Magnitude = Magnitude;
	PendingOp.OpType = OpType;

	ArcModifiersPrivate::SendPendingOpsSignal(EntityManager, Target);
}

} // anonymous namespace

namespace ArcModifiers
{

void QueueInstant(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                  const FArcAttributeRef& Attribute, EArcModifierOp Op,
                  float Magnitude)
{
	QueueOpInternal(EntityManager, Target, Attribute, Op, Magnitude, EArcAttributeOpType::Change);
}

void QueueExecute(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                  const FArcAttributeRef& Attribute, EArcModifierOp Op,
                  float Magnitude)
{
	QueueOpInternal(EntityManager, Target, Attribute, Op, Magnitude, EArcAttributeOpType::Execute);
}

FArcModifierHandle ApplyInfinite(FMassEntityManager& EntityManager,
                                 FMassEntityHandle Target, FMassEntityHandle Source,
                                 const FArcDirectModifier& Modifier)
{
	if (!EntityManager.IsEntityValid(Target) || !Modifier.Attribute.IsValid())
	{
		return FArcModifierHandle();
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcAggregatorFragment* AggregatorFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
	if (!AggregatorFrag)
	{
		return FArcModifierHandle();
	}

	FArcModifierHandle Handle = GenerateHandle();

	// OwnedModifiers is bookkeeping only — used by RemoveModifier and the debugger
	// (ArcGameplayDebugger/ArcMassAbilitiesDebugger_Attributes.cpp). Nothing references
	// its memory after this point; AddMod copies the tag reqs from the function parameter.
	AggregatorFrag->OwnedModifiers.Add(Handle, Modifier);

	FArcAggregator& Aggregator = AggregatorFrag->FindOrAddAggregator(Modifier.Attribute);
	Aggregator.AddMod(Modifier.Operation, Modifier.Magnitude, Source,
	                  &Modifier.SourceTagReqs, &Modifier.TargetTagReqs,
	                  Handle);

	ArcModifiersPrivate::SendRecalculateSignal(EntityManager, Target);

	return Handle;
}

void RemoveModifier(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                    FArcModifierHandle Handle)
{
	if (!Handle.IsValid() || !EntityManager.IsEntityValid(Target))
	{
		return;
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcAggregatorFragment* AggregatorFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
	if (!AggregatorFrag)
	{
		return;
	}

	AggregatorFrag->RemoveMod(Handle);
	AggregatorFrag->OwnedModifiers.Remove(Handle);

	ArcModifiersPrivate::SendRecalculateSignal(EntityManager, Target);
}

} // namespace ArcModifiers
