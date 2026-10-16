// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExternalSubsystemTraits.h"

#include "ArcCombatSlots.generated.h"

struct FMassEntityManager;

USTRUCT(meta = (ToolTip = "One attacker's claimed approach angle around a target."))
struct ARCAI_API FArcCombatSlotClaim
{
	GENERATED_BODY()

	UPROPERTY(meta = (ToolTip = "Entity holding this slot."))
	FMassEntityHandle Claimer;

	UPROPERTY(meta = (ToolTip = "World yaw in degrees from the target location to the slot point."))
	float YawDegrees = 0.0f;

	UPROPERTY(meta = (ToolTip = "Distance in cm from the target location to the slot point."))
	float Radius = 0.0f;

	UPROPERTY(meta = (ToolTip = "2D distance in cm from the slot point within which the claimer keeps the claim live without renewing."))
	float HoldDistance = 0.0f;

	UPROPERTY(meta = (ToolTip = "World time in seconds of the claim or its last renewal."))
	double LastRenewTime = 0.0;
};

USTRUCT(meta = (ToolTip = "Combat slots claimed around this entity by its attackers. Added on the first claim."))
struct ARCAI_API FArcCombatSlotsFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY(meta = (ToolTip = "At most one claim per claimer."))
	TArray<FArcCombatSlotClaim> Claims;
};

template<>
struct TMassFragmentTraits<FArcCombatSlotsFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

namespace ArcCombatSlots
{
	inline constexpr double LeaseSeconds = 1.0;

	ARCAI_API bool GetTargetCentre(const FMassEntityManager& EntityManager, FMassEntityHandle Target, FVector& OutCentre);

	ARCAI_API bool GetLiveSlotPoint(
		const FMassEntityManager& EntityManager,
		FMassEntityHandle Claimer,
		FMassEntityHandle Target,
		double Now,
		FVector& OutSlotPoint);

	ARCAI_API int32 CountLiveClaimers(
		const FMassEntityManager& EntityManager,
		FMassEntityHandle Target,
		FMassEntityHandle Excluding,
		double Now);

	ARCAI_API bool IsAngleFree(
		const FMassEntityManager& EntityManager,
		FMassEntityHandle Target,
		float YawDegrees,
		float MinSeparationDegrees,
		FMassEntityHandle Excluding,
		double Now);

	ARCAI_API void PushClaim(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Target,
		FMassEntityHandle Claimer,
		float YawDegrees,
		float Radius,
		float HoldDistance,
		float MinSeparationDegrees,
		int32 MaxClaimers,
		double Now);

	ARCAI_API void PushRenew(FMassEntityManager& EntityManager, FMassEntityHandle Target, FMassEntityHandle Claimer, double Now);

	ARCAI_API void PushRelease(FMassEntityManager& EntityManager, FMassEntityHandle Target, FMassEntityHandle Claimer);
}
