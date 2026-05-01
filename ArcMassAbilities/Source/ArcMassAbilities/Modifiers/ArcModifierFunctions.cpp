// Copyright Lukasz Baran. All Rights Reserved.

#include "Modifiers/ArcModifierFunctions.h"

#include "MassEntityManager.h"
#include "Attributes/ArcAttribute.h"
#include "Modifiers/ArcDirectModifier.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "MassEntityView.h"
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

	FMassEntityView View(EntityManager, Entity);
	FStructView FragView = View.GetFragmentDataStruct(Attribute.FragmentType);
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
	FArcPendingAttributeOpsFragment* Pending = View.GetFragmentDataPtr<FArcPendingAttributeOpsFragment>();
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

	return Agg->EvaluateWithTags(BaseValue, SourceTags, TargetTags);
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
	FArcPendingAttributeOpsFragment* Pending = View.GetFragmentDataPtr<FArcPendingAttributeOpsFragment>();
	if (!Pending)
	{
		return;
	}

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

void ApplyInstant(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                  const FArcAttributeRef& Attribute, EArcModifierOp Op,
                  float Magnitude)
{
	QueueInstant(EntityManager, Target, Attribute, Op, Magnitude);
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

	FArcDirectModifier& StoredModifier = AggregatorFrag->OwnedModifiers.Add(Handle, Modifier);

	FArcAggregator& Aggregator = AggregatorFrag->FindOrAddAggregator(Modifier.Attribute);
	Aggregator.AddMod(Modifier.Operation, Modifier.Magnitude, Source,
	                  &StoredModifier.SourceTagReqs, &StoredModifier.TargetTagReqs,
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

	for (TPair<FArcAttributeRef, FArcAggregator>& Pair : AggregatorFrag->Aggregators)
	{
		Pair.Value.RemoveMod(Handle);
	}

	AggregatorFrag->OwnedModifiers.Remove(Handle);

	ArcModifiersPrivate::SendRecalculateSignal(EntityManager, Target);
}

} // namespace ArcModifiers
