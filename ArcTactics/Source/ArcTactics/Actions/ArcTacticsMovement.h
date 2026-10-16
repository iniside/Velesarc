// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassMovementTypes.h"

#include "ArcTacticsMovement.generated.h"

namespace ArcPathFollow
{
	struct FContext;
}

class UArcTQSQuerySubsystem;

enum class EArcTacticsActionStatus : uint8
{
	Running,
	Succeeded,
	Failed
};

USTRUCT(BlueprintType, meta = (ToolTip = "Path-follow settings for approaching a destination: speed, corridor, arrival, repathing and timeout."))
struct ARCTACTICS_API FArcTacticsMovementSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ToolTip = "Mass movement style that sets the desired speed."))
	FMassMovementStyleRef MovementStyle;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "Multiplier applied to the movement style speed."))
	float SpeedScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "Width of the path corridor in cm."))
	float CorridorWidth = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "Distance in cm the path keeps from the corridor boundaries."))
	float OffsetFromBoundaries = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "End-of-path distance threshold in cm used when building the short path."))
	float EndDistanceThreshold = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "Distance in cm the path stops short of the destination, End Distance Threshold included. The approach succeeds when the path is done and the destination has not moved by Repath Distance. With a Destination Query, Query Approach Distance replaces it."))
	float ArrivalRange = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "Minimum seconds between path requests."))
	float RepathInterval = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "Distance in cm the target must move from the last requested destination to trigger a repath."))
	float RepathDistance = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "Seconds allowed for the approach before it fails."))
	float ApproachTimeout = 10.0f;

	/** Tiled influence grid sampled for enemy visibility while pathing. INDEX_NONE leaves paths uncosted by visibility. */
	UPROPERTY(EditAnywhere, Category = "Movement")
	int32 VisibilityGridIndex = INDEX_NONE;

	/** Extra cost of a cell seen by another team, as a fraction of its base cost: 1 doubles it. */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0"))
	float VisibilityCostMultiplier = 1.0f;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Angle tolerance and timeout for turning to face the target."))
struct ARCTACTICS_API FArcTacticsFacingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Facing", meta = (ClampMin = "0.0", ClampMax = "180.0", ToolTip = "Largest horizontal angle in degrees between the entity forward and the target direction that counts as facing."))
	float FacingToleranceDegrees = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Facing", meta = (ClampMin = "0.0", ToolTip = "Seconds allowed to finish turning before the step fails."))
	float FacingTimeout = 1.0f;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsMovementHoldState
{
	GENERATED_BODY()

	uint16 ActionID = 0;
	bool bOwned = false;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsMovementRuntimeData
{
	GENERATED_BODY()

	FVector RequestedDestination = FVector::ZeroVector;
	FVector DestinationQueryAnchor = FVector::ZeroVector;
	FVector ResolvedDestination = FVector::ZeroVector;
	double NextRepathTime = 0.0;
	double ApproachDeadline = 0.0;
	double FacingDeadline = 0.0;
	uint16 OwnedActionID = 0;
	int32 DestinationQueryID = INDEX_NONE;
	bool bApproachDeadlineSet = false;
	bool bOwnsMove = false;
	bool bOwnsStand = false;
	bool bFacingStarted = false;
	bool bHasDestinationQueryAnchor = false;
	bool bHasResolvedDestination = false;
	bool bNavPathPartial = false;
};

namespace ArcTacticsMovement
{
	ARCTACTICS_API bool Validate(const FArcTacticsMovementSettings& Settings, FString& OutError);

	ARCTACTICS_API bool ValidateFacing(const FArcTacticsFacingSettings& Settings, FString& OutError);

	ARCTACTICS_API EArcTacticsActionStatus StartApproach(
		const ArcPathFollow::FContext& PathContext,
		const FArcTacticsMovementSettings& Settings,
		const FVector& TargetLocation,
		FArcTacticsMovementRuntimeData& RuntimeData,
		FArcTacticsMovementHoldState& HoldState,
		uint32 ActionSerial);

	ARCTACTICS_API EArcTacticsActionStatus UpdateApproach(
		const ArcPathFollow::FContext& PathContext,
		const FArcTacticsMovementSettings& Settings,
		const FVector& TargetLocation,
		FArcTacticsMovementRuntimeData& RuntimeData,
		FArcTacticsMovementHoldState& HoldState,
		uint32 ActionSerial);

	ARCTACTICS_API EArcTacticsActionStatus StartFacing(
		const ArcPathFollow::FContext& PathContext,
		const FArcTacticsFacingSettings& Settings,
		const FVector& TargetLocation,
		FArcTacticsMovementRuntimeData& RuntimeData,
		FArcTacticsMovementHoldState& HoldState);

	ARCTACTICS_API EArcTacticsActionStatus UpdateFacing(
		const ArcPathFollow::FContext& PathContext,
		const FArcTacticsFacingSettings& Settings,
		const FVector& TargetLocation,
		FArcTacticsMovementRuntimeData& RuntimeData);

	ARCTACTICS_API bool Stop(
		const ArcPathFollow::FContext& PathContext,
		FArcTacticsMovementRuntimeData& RuntimeData,
		FArcTacticsMovementHoldState& HoldState);

	ARCTACTICS_API bool AcquireHold(
		const ArcPathFollow::FContext& PathContext,
		FArcTacticsMovementRuntimeData& RuntimeData,
		FArcTacticsMovementHoldState& HoldState);

	ARCTACTICS_API bool IsHoldOwned(
		const ArcPathFollow::FContext& PathContext,
		const FArcTacticsMovementHoldState& HoldState);

	ARCTACTICS_API bool ReleaseHold(
		const ArcPathFollow::FContext& PathContext,
		FArcTacticsMovementRuntimeData& RuntimeData,
		FArcTacticsMovementHoldState& HoldState);

	ARCTACTICS_API void AbortDestinationQuery(
		UArcTQSQuerySubsystem* QuerySubsystem,
		FArcTacticsMovementRuntimeData& RuntimeData);

	ARCTACTICS_API void ResetActionScratch(FArcTacticsMovementRuntimeData& RuntimeData);

	ARCTACTICS_API void CancelAction(
		const ArcPathFollow::FContext& PathContext,
		FArcTacticsMovementRuntimeData& RuntimeData);

	ARCTACTICS_API void Cancel(
		const ArcPathFollow::FContext& PathContext,
		FArcTacticsMovementRuntimeData& RuntimeData,
		FArcTacticsMovementHoldState& HoldState);
}
