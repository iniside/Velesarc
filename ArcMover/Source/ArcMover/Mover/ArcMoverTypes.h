// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "MoverTypes.h"
#include "DefaultMovementSet/Modes/SmoothWalkingMode.h"
#include "Mover/ArcWalkGait.h"
#include "ArcMoverTypes.generated.h"


ARCMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Mover_Gait_Walk);
ARCMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Mover_Gait_Run);
ARCMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Mover_Gait_Sprint);

ARCMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Mover_BlockAllInput);

UENUM(BlueprintType)
enum class EArcMoverGaitType : uint8
{
	Walk,
	Run,
	Sprint
};

UENUM(BlueprintType)
enum class EArcMoverAimModeType : uint8
{
	OrientToMovement,
	Strafe,
	Aim
};

UENUM(BlueprintType)
enum class EArcMoverDirectionType : uint8
{
	F,
	B,
	LL,
	LR,
	RL,
	RR
};

/** Please add a struct description */
USTRUCT(BlueprintType)
struct FArcMoverCustomInputs : public FMoverDataStructBase
{
	GENERATED_BODY()
	
public:
	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EArcMoverAimModeType RotationMode = EArcMoverAimModeType::Strafe;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EArcMoverGaitType Gait  = EArcMoverGaitType::Run;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EArcMoverDirectionType MovementDirection = EArcMoverDirectionType::F;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float MaxVelocityMultiplier = 1.0f;
	
	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double RotationOffset = 0.0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double ControlRotationRate  = 0.0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool WantsToCrouch = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bHaveFocusTarget = false;
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMoverCustomInputs::StaticStruct();
	}
	
	virtual FMoverDataStructBase* Clone() const override
	{
		return new FArcMoverCustomInputs(*this);
	}

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		const FArcMoverCustomInputs& TypedAuthority = static_cast<const FArcMoverCustomInputs&>(AuthorityState);
		return RotationMode != TypedAuthority.RotationMode
			|| Gait != TypedAuthority.Gait
			|| MovementDirection != TypedAuthority.MovementDirection
			|| MaxVelocityMultiplier != TypedAuthority.MaxVelocityMultiplier
			|| RotationOffset != TypedAuthority.RotationOffset
			|| ControlRotationRate != TypedAuthority.ControlRotationRate
			|| WantsToCrouch != TypedAuthority.WantsToCrouch
			|| bHaveFocusTarget != TypedAuthority.bHaveFocusTarget;
	}

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override
	{
		const FArcMoverCustomInputs& TypedFrom = static_cast<const FArcMoverCustomInputs&>(From);
		const FArcMoverCustomInputs& TypedTo = static_cast<const FArcMoverCustomInputs&>(To);
		const FArcMoverCustomInputs& ClosestInputs = Pct < 1.0f ? TypedFrom : TypedTo;

		RotationMode = ClosestInputs.RotationMode;
		Gait = ClosestInputs.Gait;
		MovementDirection = ClosestInputs.MovementDirection;
		WantsToCrouch = ClosestInputs.WantsToCrouch;
		bHaveFocusTarget = ClosestInputs.bHaveFocusTarget;

		MaxVelocityMultiplier = FMath::Lerp(TypedFrom.MaxVelocityMultiplier, TypedTo.MaxVelocityMultiplier, Pct);
		RotationOffset = FRotator3d::NormalizeAxis(TypedFrom.RotationOffset + FMath::FindDeltaAngleDegrees(TypedFrom.RotationOffset, TypedTo.RotationOffset) * Pct);
		ControlRotationRate = FMath::Lerp(TypedFrom.ControlRotationRate, TypedTo.ControlRotationRate, static_cast<double>(Pct));
	}

	virtual void Merge(const FMoverDataStructBase& From) override
	{
		const FArcMoverCustomInputs& TypedFrom = static_cast<const FArcMoverCustomInputs&>(From);
		WantsToCrouch |= TypedFrom.WantsToCrouch;
		bHaveFocusTarget |= TypedFrom.bHaveFocusTarget;
	}
};
template<>
struct TStructOpsTypeTraits< FArcMoverCustomInputs > : public TStructOpsTypeTraitsBase2< FArcMoverCustomInputs >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};

USTRUCT(BlueprintType)
struct ARCMOVER_API FArcMovementModifier_MaxVelocity : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxVelocity = 100.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EArcMoverGaitType Gait = EArcMoverGaitType::Walk;
	
	float OriginalMaxSpeed = 0.f;
	
	FArcMovementModifier_MaxVelocity();
	virtual ~FArcMovementModifier_MaxVelocity() override = default;

	/** Fired when this modifier is activated. */
	virtual void OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
	
	/** Fired when this modifier is deactivated. */
	virtual void OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
	
	// @return newly allocated copy of this FMovementModifier. Must be overridden by child classes
	virtual FMovementModifierBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

};

template<>
struct TStructOpsTypeTraits< FArcMovementModifier_MaxVelocity > : public TStructOpsTypeTraitsBase2< FArcMovementModifier_MaxVelocity >
{
	enum
	{
		WithCopy = true
	};
};

UCLASS()
class UArcMoverFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "ArcCore|Mover")
	static FMovementModifierHandle ApplyMaxVelocity(AActor* Target, float MaxVelocity, EArcMoverGaitType Gait);
};

UCLASS(BlueprintType, Experimental)
class ARCMOVER_API UArcSmoothWalkingMode : public USmoothWalkingMode, public IArcWalkGaitProvider
{
	GENERATED_BODY()

public:
	UArcSmoothWalkingMode(const FObjectInitializer& ObjectInitializer);

	virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds, const FMoverSimContext& SimContext, const FVector& DesiredVelocity,
									 const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) override;

	virtual FArcWalkGaitSettings GetGaitSettings() const override;
	virtual float GetBaseMaxSpeed() const override;
	virtual float GetDecelerationSmoothingTime() const override;
	virtual float GetBrakingLag() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WalkSpeed = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float WalkAcceleration = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float RunSpeed = 375.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float RunAcceleration = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SprintSpeed = 585.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SprintAcceleration = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float CrouchSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0"))
	float WalkRunTurnStrength = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0"))
	float SprintTurnStrength = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float GaitChangeDeceleration = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float StoppingDeceleration = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "s"))
	float WalkRunFacingTime = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "s"))
	float SprintFacingTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleFacingTime = 0.2f;

private:
	double JustLandedUntilSimTimeMs = -1.0;
	mutable double CurrentSimTimeMs = 0.0;
};


