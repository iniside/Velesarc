// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Modes/ChaosCharacterMovementMode.h"
#include "UObject/Object.h"
#include "ChaosNavWalkingMode.generated.h"

class UCommonLegacyMovementSettings;
class UNavMoverComponent;
class INavigationDataInterface;
struct FFloorCheckResult;
struct FWaterCheckResult;
/**
 * 
 */
UCLASS()
class ARCCORE_API UChaosNavWalkingMode : public UChaosCharacterMovementMode
{
	GENERATED_BODY()
	
public:
	UChaosNavWalkingMode(const FObjectInitializer& ObjectInitializer);

	// Damping factor to control the softness of the interaction between the character and the ground
	// Set to 0 for no damping and 1 for maximum damping
	UPROPERTY(EditAnywhere, Category = "Constraint Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float GroundDamping = 0.0f;

	// Maximum force the character can apply to hold in place while standing on an unwalkable incline
	UPROPERTY(EditAnywhere, Category = "Constraint Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "Newtons"))
	float FrictionForceLimit = 100.0f;

	// Scaling applied to the radial force limit to raise the limit to always allow the character to
	// reach the motion target/
	// A value of 1 means that the radial force limit will be increased as needed to match the force
	// required to achieve the motion target.
	// A value of 0 means no scaling will be applied.
	UPROPERTY(EditAnywhere, Category = "Constraint Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalRadialForceLimitScaling = 1.0f;

	// Controls the reaction force applied to the ground in the ground plane when the character is moving
	// A value of 1 means that the full reaction force is applied
	// A value of 0 means the character only applies a normal force to the ground and no tangential force
	UPROPERTY(EditAnywhere, Category = "Constraint Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalGroundReaction = 1.0f;

	// Controls how much downward velocity is applied to keep the character rooted to the ground when the character
	// is within MaxStepHeight of the ground surface.
	UPROPERTY(EditAnywhere, Category = "Movement Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalDownwardVelocityToTarget = 1.0f;

	UPROPERTY(Category = "NavMesh Movement", EditAnywhere, BlueprintReadWrite)
	bool bProjectNavMeshWalking;

	UPROPERTY(Category = "NavMesh Movement", EditAnywhere, BlueprintReadWrite, meta = (editcondition = "bProjectNavMeshWalking", ClampMin = "0", UIMin = "0"))
	float NavMeshProjectionInterpSpeed = 0;
	
	UPROPERTY(Category = "NavMesh Movement", EditAnywhere, BlueprintReadWrite, meta = (editcondition = "bProjectNavMeshWalking", ForceUnits = "s"))
	float NavMeshProjectionInterval = 1;
	
	UPROPERTY(Transient)
	float NavMeshProjectionTimer;
	
	UPROPERTY(Category = "NavMesh Movement", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	uint8 bProjectNavMeshOnBothWorldChannels : 1;
	
	/**
	 * If true, walking movement always maintains horizontal velocity when moving up ramps, which causes movement up ramps to be faster parallel to the ramp surface.
	 * If false, then walking movement maintains velocity magnitude parallel to the ramp surface.
	 */
	UPROPERTY(EditAnywhere, Category = "Movement Settings")
	uint8 bMaintainHorizontalGroundVelocity : 1 = 0;

	TWeakObjectPtr<const UCommonLegacyMovementSettings> CommonLegacySettings;
	/** Associated Movement component that will actually move the actor */
	TWeakObjectPtr<UNavMoverComponent> NavMoverComponent;

	// Note: This isn't guaranteed to be valid at all times. It will be most of the time but re-entering this mode to call Activate() will search for NavData again and update it accordingly.
	TWeakInterfacePtr<const INavigationDataInterface> NavDataInterface;
	
	virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraintSettings& ConstraintSettings) const override;

	virtual void Activate(const FMoverEventContext& Context) override;
	virtual void GenerateMove_Implementation(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	virtual void ModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier) const override;

protected:
	const INavigationDataInterface* GetNavData() const;

	/** Performs trace for ProjectLocationFromNavMesh */
	virtual void FindBestNavMeshLocation(const FVector& TraceStart, const FVector& TraceEnd, const FVector& CurrentFeetLocation, const FVector& TargetNavLocation, FHitResult& OutHitResult) const;

	/**
	 * Attempts to better align navmesh walking actors with underlying geometry (sometimes
	 * navmesh can differ quite significantly from geometry).
	 * Updates CachedProjectedNavMeshHitResult, access this for more info about hits.
	 */
	virtual FVector ProjectLocationFromNavMesh(float DeltaSeconds, const FVector& CurrentFeetLocation, const FVector& TargetNavLocation, float UpOffset, float DownOffset);


	virtual void OnRegistered(const FName ModeName) override;
	virtual void OnUnregistered() override;

	void CaptureOutputState(const FMoverDefaultSyncState& StartSyncState, const FVector& FinalLocation, const FRotator& FinalRotation, const FMovementRecord& Record, const FVector& AngularVelocityDegrees, FMoverDefaultSyncState& OutputSyncState, FMoverTickEndData& OutputState) const;
	
	virtual bool CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult, const FVector& CharacterVelocity) const;
	virtual void GetFloorAndCheckMovement(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, const FChaosMoverSimulationDefaultInputs& DefaultSimInputs, float DeltaSeconds, FFloorCheckResult& FloorResult, FWaterCheckResult& WaterResult, FVector& OutDeltaPos) const;
};
