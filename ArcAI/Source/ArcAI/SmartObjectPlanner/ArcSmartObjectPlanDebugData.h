// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

#if !UE_BUILD_SHIPPING

enum class EArcPlanCandidateRejection : uint8
{
	None,
	NoSlots,
	CustomConditionFailed,
	NoNewTags,
	RequirementConflict,
	RequirementUnsatisfiable,
};

struct FArcPlanCandidateDebugEntry
{
	FMassEntityHandle EntityHandle;
	FVector Location = FVector::ZeroVector;
	FGameplayTagContainer Provides;
	FGameplayTagContainer Requires;
	int32 SlotCount = 0;
	EArcPlanCandidateRejection Rejection = EArcPlanCandidateRejection::None;
	FString RejectionDetail;
	FString SensorSource;
};

struct FArcSmartObjectPlanDebugData
{
	// Query parameters
	FVector SearchOrigin = FVector::ZeroVector;
	float SearchRadius = 0.f;
	FGameplayTagContainer RequiredTags;
	FGameplayTagContainer InitialTags;

	// Candidates after sensor gather + dedup
	TArray<FArcPlanCandidateDebugEntry> Candidates;

	// Results
	int32 PlansFound = 0;
	bool bHitDepthLimit = false;

	// Tags that no candidate could provide
	FGameplayTagContainer UnsatisfiedTags;

	void Reset()
	{
		SearchOrigin = FVector::ZeroVector;
		SearchRadius = 0.f;
		RequiredTags.Reset();
		InitialTags.Reset();
		Candidates.Reset();
		PlansFound = 0;
		bHitDepthLimit = false;
		UnsatisfiedTags.Reset();
	}
};

#endif // !UE_BUILD_SHIPPING
