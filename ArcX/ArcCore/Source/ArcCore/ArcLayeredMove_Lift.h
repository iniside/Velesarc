// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "LayeredMoveBase.h"
#include "MovementModifier.h"
#include "ArcLayeredMove_Lift.generated.h"

class UCurveVector;
class UCurveFloat;

/**
 * 
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcLayeredMove_Lift : public FLayeredMoveBase
{
	GENERATED_BODY()

	FArcLayeredMove_Lift();
	virtual ~FArcLayeredMove_Lift() override = default;

	// How Long target will stay lift when it reached TargetLocation. Must be smaller than Move Duration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float LiftDurationMs;
	
	// Location to Start the MoveTo move from
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector StartLocation;
	
	// Location to move towards
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector TargetLocation;

	// if true, will restrict speed to where the actor is expected to be (in regard to start, end and duration)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bRestrictSpeedToExpected;
	
	// Optional CurveVector used to offset the actor from the path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveVector> PathOffsetCurve;

	// Optional CurveFloat to apply to how fast the actor moves as they get closer to the target location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveFloat> TimeMappingCurve;
	
	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	
	virtual bool GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	
	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	
protected:
	// helper function to apply movement vector offset from the PathOffsetCurve
	FVector GetPathOffsetInWorldSpace(const float MoveFraction) const;

	// Helper function to apply TimeMappingCurve to the layered move
	float EvaluateFloatCurveAtFraction(const UCurveFloat& Curve, const float Fraction) const;
};

template<>
struct TStructOpsTypeTraits< FArcLayeredMove_Lift > : public TStructOpsTypeTraitsBase2< FArcLayeredMove_Lift >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcLayeredMove_NoVelocity : public FLayeredMoveBase
{
	GENERATED_BODY()

	FArcLayeredMove_NoVelocity();
	virtual ~FArcLayeredMove_NoVelocity() override = default;

	virtual bool GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	
	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits< FArcLayeredMove_NoVelocity > : public TStructOpsTypeTraitsBase2< FArcLayeredMove_NoVelocity >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};

USTRUCT(MinimalAPI, BlueprintType)
struct FArcChaosModifier_MaxSpeed : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	FArcChaosModifier_MaxSpeed();
	virtual ~FArcChaosModifier_MaxSpeed() override = default;
	
	/** Override Max Speed */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	TOptional<float> MaxSpeedOverride;

	virtual void OnStart_Async(const FMovementModifierParams_Async& Params) override;
	virtual void OnEnd_Async(const FMovementModifierParams_Async& Params) override;
	virtual void OnPostMovement_Async(const FMovementModifierParams_Async& Params) override;

	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;

};

template<>
struct TStructOpsTypeTraits< FArcChaosModifier_MaxSpeed > : public TStructOpsTypeTraitsBase2< FArcChaosModifier_MaxSpeed >
{
	enum
	{
		WithCopy = true
	};
};
