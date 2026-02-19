// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "MoverTypes.h"
#include "DefaultMovementSet/Modes/SmoothWalkingMode.h"
#include "ArcMoverTypes.generated.h"


ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Mover_Gait_Walk);
ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Mover_Gait_Run);
ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Mover_Gait_Sprint);

ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Mover_BlockAllInput);

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
struct FArcMovementModifier_MaxVelocity : public FMovementModifierBase
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

USTRUCT()
struct FArcSmoothWalkingState : public FMoverDataStructBase
{
	GENERATED_BODY()
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FMoverDataStructBase* Clone() const override;
	//virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	//virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	//virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	//virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;

	// Velocity of internal velocity spring
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Experimental")
	float SmoothTime = 0.0f;
};


// Try to replace entire thing with chaos..
UCLASS(BlueprintType, Experimental)
class UArcSmoothWalkingMode : public USmoothWalkingMode
{
	GENERATED_BODY()
	
public:
	UArcSmoothWalkingMode(const FObjectInitializer& ObjectInitializer);
	
	virtual void GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity,
								 const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) override;
};


