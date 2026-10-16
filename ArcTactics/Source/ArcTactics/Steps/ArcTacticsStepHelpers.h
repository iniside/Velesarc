// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsMovement.h"
#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

struct FArcTacticsActionContext;
struct FArcTacticsUseAbilityStepInstanceData;

namespace ArcTacticsStepHelpers
{
	bool ResolveTargetLocation(
		const FArcTacticsActionContext& Context,
		const FMassEntityHandle Target,
		const FVector& TargetLocation,
		const bool bUseTargetLocation,
		FVector& OutLocation);

	void CancelExactAbility(FArcTacticsActionContext& Context, FArcTacticsUseAbilityStepInstanceData& Data);

	EArcTacticsActionStatus TickAbility(
		FArcTacticsActionContext& Context,
		FArcTacticsUseAbilityStepInstanceData& Data,
		const uint8 AcquireHoldStage,
		const uint8 WaitingStage,
		const uint8 UsingStage);

	void ResetAbilityScratch(FArcTacticsUseAbilityStepInstanceData& Data);

	EArcTacticsActionStatus FinishAbilityStep(
		FArcTacticsActionContext& Context,
		FArcTacticsUseAbilityStepInstanceData& Data,
		const EArcTacticsActionStatus Status);
}
