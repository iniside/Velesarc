// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"

struct FMassEntityManager;
struct FMassEntityHandle;
enum class EArcModifierOp : uint8;
struct FArcAttributeRef;

namespace ArcAttributes
{
	/**
	 * Inject a one-shot modifier into the same pipeline that instant effects use:
	 * appends an FArcPendingAttributeOp onto the entity's FArcPendingAttributeOpsFragment
	 * and fires the PendingAttributeOps signal. UArcPendingAttributeOpsProcessor drains
	 * on the next signal pass, applying the op to BaseValue, running the aggregator,
	 * and firing PreChange / PostChange (+ PostExecute when OpType == Execute) via
	 * NotifyAttributeChanged / NotifyAttributeExecuted.
	 *
	 * Returns false if the entity lacks FArcPendingAttributeOpsFragment.
	 */
	ARCMASSABILITIES_API bool EnqueueInstantModifier(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcAttributeRef& AttrRef,
		EArcModifierOp Operation,
		float Magnitude,
		EArcAttributeOpType OpType = EArcAttributeOpType::Execute,
		FMassEntityHandle SourceEntity = FMassEntityHandle(),
		const FGameplayTagContainer& CapturedSourceTags = FGameplayTagContainer(),
		const FGameplayTagContainer& CapturedTargetTags = FGameplayTagContainer());
}
