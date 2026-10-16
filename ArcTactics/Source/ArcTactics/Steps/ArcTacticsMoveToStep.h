// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsAction.h"
#include "Actions/ArcTacticsMovement.h"
#include "ArcTQSQueryReference.h"
#include "CoreMinimal.h"
#include "Decision/ArcTacticsNode.h"
#include "Mass/EntityHandle.h"

#include "ArcTacticsMoveToStep.generated.h"

struct FArcTQSQueryInstance;

USTRUCT(BlueprintType, meta = (ToolTip = "Inputs, parameters and runtime state of a Move To step."))
struct ARCTACTICS_API FArcTacticsMoveToStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "Target entity whose current position is the target location unless Use Target Location is set."))
	FMassEntityHandle Target;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "World location in cm used as the target location when Use Target Location is set."))
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "World location in cm the entity travels to when Use Destination is set."))
	FVector Destination = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Travels to Destination instead of the target location."))
	bool bUseDestination = false;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Uses Target Location as the target location instead of the target entity position."))
	bool bUseTargetLocation = false;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Pathing, arrival, repath and timeout settings for the approach."))
	FArcTacticsMovementSettings Movement;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Optional query that picks the destination; the entity paths to its first result. The query receives the target through its parameters."))
	FArcTQSQueryReference DestinationQuery;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ClampMin = "0.0", Units = "cm", ToolTip = "With a Destination Query, replaces Movement Arrival Range: the distance in cm the path stops short of the query result."))
	float QueryApproachDistance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "With a Target and a Destination Query, claims the query result as a combat slot on the target and follows that slot around the moving target. Arrival is the path to the slot point finishing, stopped short by Query Approach Distance."))
	bool bClaimCombatSlot = false;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, EditCondition = "bClaimCombatSlot", ClampMin = "0.0", Units = "cm", ToolTip = "2D distance in cm from the slot point within which the entity keeps its claim live without renewing it."))
	float SlotHoldDistance = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, EditCondition = "bClaimCombatSlot", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees", ToolTip = "The claim is rejected when another live slot on the target is within this many degrees. Should match the query's Combat Slot Free step."))
	float SlotMinSeparationDegrees = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, EditCondition = "bClaimCombatSlot", ClampMin = "1", ToolTip = "The claim is rejected when this many other attackers already hold live slots on the target. Should match the Combat Slots Available consideration's Max Attackers."))
	int32 SlotMaxClaimers = 3;

	UPROPERTY()
	FArcTacticsMovementRuntimeData MovementRuntime;

	UPROPERTY()
	bool bDestinationQueryCompleted = false;

	UPROPERTY()
	bool bDestinationQuerySucceeded = false;

	UPROPERTY(meta = (ToolTip = "World time in seconds the pending combat slot claim was pushed."))
	double ClaimPushTime = 0.0;

	UPROPERTY(meta = (ToolTip = "World time in seconds of the last combat slot renewal push."))
	double LastRenewPushTime = 0.0;

	UPROPERTY(meta = (ToolTip = "A combat slot claim was pushed and is not yet readable on the target."))
	bool bClaimPending = false;

	UPROPERTY(meta = (ToolTip = "The step is following its live combat slot on the target."))
	bool bFollowingClaim = false;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Move To", ToolTip = "Paths to the destination or the target location, stopped short by Arrival Range, and succeeds when the path is done."))
struct ARCTACTICS_API FArcTacticsMoveToStep : public FArcTacticsStepNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual bool Enter(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
	virtual void Cancel(
		FArcTacticsStepContext& Context,
		FStructView InstanceData,
		EArcTacticsActionCancelMode CancelMode) const override;
	virtual void OnAsyncQueryCompleted(
		FArcTacticsStepContext& Context,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken& Token,
		FArcTQSQueryInstance& CompletedQuery) const override;
	virtual EArcTacticsActionTickLane GetTickLane(FConstStructView InstanceData) const override;
};
