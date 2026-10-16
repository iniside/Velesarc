// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "ChaosMover/Character/Modes/ChaosWalkingMode.h"
#include "ChaosMover/Character/Sources/ChaosCharacterSmoothWalkingSource.h"
#include "MoverTypes.h"
#include "ArcChaosSlide.generated.h"

namespace ArcMoverModeNames
{
	extern ARCMOVER_API const FName Sliding;
}

USTRUCT()
struct ARCMOVER_API FArcChaosSlideState : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY()
	double ActivatedSimTimeMs = 0.0;

	UPROPERTY()
	float LastComputedMaxSpeed = 0.0f;

	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FMoverDataStructBase* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

template<>
struct TStructOpsTypeTraits<FArcChaosSlideState> : public TStructOpsTypeTraitsBase2<FArcChaosSlideState>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UArcChaosSlideSource : public UChaosCharacterSmoothWalkingSource
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosSlideSource(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "s"))
	float InitialBoostTime = 0.2f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float InitialBoostSpeed = 800.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float InitialBoostAcceleration = 2000.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float AfterBoostAcceleration = 300.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ForceUnits = "deg"))
	float SteepSlopeAngle = 40.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ForceUnits = "deg"))
	float ShallowSlopeAngle = 10.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SteepSlopeSpeed = 800.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float ShallowSlopeSpeed = 500.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float FlatGroundSpeed = 100.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SteepSlopeDeceleration = 2000.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float FlatGroundDeceleration = 500.0f;

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ForceUnits = "cm/s"))
	float OwnVelocityFeedThreshold = 15.0f;

	ARCMOVER_API virtual void GenerateMove_Async(
		const FMoverTickStartData& StartState,
		const FMoverTimeStep& TimeStep,
		UMoverBlackboard* SimBlackboard,
		FProposedMove& OutProposedMove) override;

	ARCMOVER_API virtual TConstArrayView<const UScriptStruct*> GetPersistentSyncStateTypes() const override;

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
	double CurrentStepSimTimeMs = 0.0;
};

UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UArcChaosSlideMode : public UChaosWalkingMode
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosSlideMode(const FObjectInitializer& ObjectInitializer);

	ARCMOVER_API virtual void Activate(
		const FMoverEventContext& Context,
		FName PrevModeName,
		const FMoverSimContext& SimContext,
		const FMoverTickStartData& StartState,
		FMoverSyncState* OutSyncState,
		FMoverAuxStateContext* OutAuxState) override;

	ARCMOVER_API virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
};

UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UArcChaosToSlideTransition : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosToSlideTransition(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float EnterSpeed = 380.0f;

	ARCMOVER_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
};

UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UArcChaosFromSlideTransition : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosFromSlideTransition(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(Category = "Slide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float ExitSpeed = 200.0f;

	ARCMOVER_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
};
