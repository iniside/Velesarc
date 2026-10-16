// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "Targeting/ArcTabTargetingMessages.h"
#include "Targeting/ArcTabTargetingTypes.h"

struct FArcTabTargetingFragment;
struct FArcTargetCandidate;
struct FMassEntityManager;
class UArcTargetingSubsystem;
class UWorld;

ARCABILITYKIT_API DECLARE_LOG_CATEGORY_EXTERN(LogArcTabTargeting, Log, All);

namespace ArcTabTargetingState
{
	ARCABILITYKIT_API bool IsSelectable(
		FMassEntityManager& EntityManager,
		const FMassEntityHandle SourceEntity,
		const FVector& SourceLocation,
		const FMassEntityHandle Target,
		const FArcTabTargetRetentionPolicy& Policy,
		FVector& OutTargetLocation,
		EArcTabTargetChangeReason& OutReason);

	ARCABILITYKIT_API const FArcTargetCandidate* GetSelectedCandidate(
		const UArcTargetingSubsystem& Subsystem,
		const FMassEntityHandle SourceEntity,
		const FGameplayTag SelectedTargetTag);

	ARCABILITYKIT_API void PublishSelectedTarget(
		UArcTargetingSubsystem& Subsystem,
		const FMassEntityHandle SourceEntity,
		const FGameplayTag SelectedTargetTag,
		const FArcTargetCandidate& Candidate,
		const FVector& TargetLocation);

	ARCABILITYKIT_API void ResetCycle(FArcTabTargetingFragment& State);

	ARCABILITYKIT_API void BroadcastTargetChanged(
		UWorld& World,
		const FMassEntityHandle SourceEntity,
		const FMassEntityHandle PreviousTarget,
		const FMassEntityHandle NewTarget,
		const EArcTabTargetChangeReason Reason);

	ARCABILITYKIT_API const TCHAR* LexReason(const EArcTabTargetChangeReason Reason);
}
