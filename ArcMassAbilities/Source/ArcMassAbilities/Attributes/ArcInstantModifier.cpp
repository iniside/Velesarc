// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcInstantModifier.h"
#include "ArcMassAbilitiesLogs.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"

namespace ArcAttributes
{

bool EnqueueInstantModifier(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const FArcAttributeRef& AttrRef,
	EArcModifierOp Operation,
	float Magnitude,
	EArcAttributeOpType OpType,
	FMassEntityHandle SourceEntity,
	const FGameplayTagContainer& CapturedSourceTags,
	const FGameplayTagContainer& CapturedTargetTags)
{
	FMassEntityView EntityView(EntityManager, Entity);
	FArcPendingAttributeOpsFragment* Pending = EntityView.GetSparseFragmentDataPtr<FArcPendingAttributeOpsFragment>();
	if (!Pending)
	{
		return false;
	}

	ARCMA_VLOG(EntityManager.GetWorld(), Log,
		TEXT("[Attr] Op queued attr=%s::%s op=%d magnitude=%.4f entity=%s"),
		AttrRef.FragmentType ? *AttrRef.FragmentType->GetName() : TEXT("<null>"),
		*AttrRef.PropertyName.ToString(),
		static_cast<int32>(Operation),
		Magnitude,
		*Entity.DebugGetDescription());

	FArcPendingAttributeOp& PendingOp = Pending->PendingOps.AddDefaulted_GetRef();
	PendingOp.Attribute = AttrRef;
	PendingOp.Operation = Operation;
	PendingOp.Magnitude = Magnitude;
	PendingOp.OpType = OpType;
	PendingOp.SourceEntity = SourceEntity;
	PendingOp.CapturedSourceTags = CapturedSourceTags;
	PendingOp.CapturedTargetTags = CapturedTargetTags;

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
	}

	return true;
}

} // namespace ArcAttributes
