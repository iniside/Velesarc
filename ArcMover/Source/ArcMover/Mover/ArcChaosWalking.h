// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Modes/ChaosWalkingMode.h"
#include "ChaosMover/Character/Sources/ChaosCharacterSmoothWalkingSource.h"
#include "Mover/ArcWalkGait.h"
#include "MoverTypes.h"
#include "ArcChaosWalking.generated.h"

class UMoverBlackboard;

namespace ArcChaosMover
{
	void CarryPersistentStateIntoStartState(const UChaosMoverSourceBase* Source, const FMoverTickStartData& StartState);
	FVector GetWalkMovementNormal(const UMoverBlackboard* Blackboard, const FVector& UpDir, bool bMaintainHorizontalGroundVelocity);
	void FeedSpringOwnVelocity(UChaosMoverSourceBase& Source, const FVector& MovementNormal, float Threshold, FVector& InOutVelocity);
	void RecordSpringOwnVelocity(UChaosMoverSourceBase& Source, const FVector& ProposedVelocity);
}

USTRUCT()
struct ARCMOVER_API FArcChaosSpringFeedState : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY()
	FVector LastProposedVelocity = FVector::ZeroVector;

	UPROPERTY()
	bool bValid = false;

	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FMoverDataStructBase* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

template<>
struct TStructOpsTypeTraits<FArcChaosSpringFeedState> : public TStructOpsTypeTraitsBase2<FArcChaosSpringFeedState>
{
	enum
	{
		WithCopy = true
	};
};

USTRUCT()
struct ARCMOVER_API FArcChaosWalkState : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY()
	double JustLandedUntilSimTimeMs = -1.0;

	UPROPERTY()
	float LastComputedMaxSpeed = -1.0f;

	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FMoverDataStructBase* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

template<>
struct TStructOpsTypeTraits<FArcChaosWalkState> : public TStructOpsTypeTraitsBase2<FArcChaosWalkState>
{
	enum
	{
		WithCopy = true
	};
};

UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UArcChaosSmoothWalkingSource : public UChaosCharacterSmoothWalkingSource, public IArcWalkGaitProvider
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosSmoothWalkingSource(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking")
	FArcWalkGaitSettings Gait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ForceUnits = "cm/s"))
	float OwnVelocityFeedThreshold = 15.0f;

	ARCMOVER_API virtual void GenerateMove_Async(
		const FMoverTickStartData& StartState,
		const FMoverTimeStep& TimeStep,
		UMoverBlackboard* SimBlackboard,
		FProposedMove& OutProposedMove) override;

	ARCMOVER_API virtual TConstArrayView<const UScriptStruct*> GetPersistentSyncStateTypes() const override;

	ARCMOVER_API virtual FArcWalkGaitSettings GetGaitSettings() const override;
	ARCMOVER_API virtual float GetBaseMaxSpeed() const override;
	ARCMOVER_API virtual float GetDecelerationSmoothingTime() const override;
	ARCMOVER_API virtual float GetBrakingLag() const override;

protected:
	ARCMOVER_API virtual void ApplyWalkMove(
		FMoverTickStartData& StartState,
		float DeltaSeconds,
		const FVector& DesiredVelocity,
		const FQuat& DesiredFacing,
		const FQuat& CurrentFacing,
		FVector& InOutAngularVelocityDegrees,
		FVector& InOutVelocity) override;

private:
	double CurrentSimTimeMs = 0.0;
};

UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UArcChaosSmoothWalkingMode : public UChaosWalkingMode
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosSmoothWalkingMode(const FObjectInitializer& ObjectInitializer);

	ARCMOVER_API virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState
		, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;

	ARCMOVER_API virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
};
