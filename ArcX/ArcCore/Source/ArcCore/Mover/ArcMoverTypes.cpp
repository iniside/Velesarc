// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMoverTypes.h"

#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"

#include "GameFramework/Actor.h"
#include "Player/ArcHeroComponentBase.h"

FArcMovementModifier_MaxVelocity::FArcMovementModifier_MaxVelocity()
{
	DurationMs = -1.0f;
}

void FArcMovementModifier_MaxVelocity::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState
	, const FMoverAuxStateContext& AuxState)
{
	if (UCommonLegacyMovementSettings* MovementSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
	{
		OriginalMaxSpeed = MovementSettings->MaxSpeed;
		MovementSettings->MaxSpeed = MaxVelocity;
	}
	
	if (UArcMoverInputProducerComponent* Producer = MoverComp->GetOwner()->FindComponentByClass<UArcMoverInputProducerComponent>())
	{
		//Producer->OverrideInput = FVector::ZeroVector;
		Producer->GaitOverride = Gait;
		Producer->bStopImmidietly = true;
	}
}

void FArcMovementModifier_MaxVelocity::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState
	, const FMoverAuxStateContext& AuxState)
{
	if (UCommonLegacyMovementSettings* MovementSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
	{
		MovementSettings->MaxSpeed = OriginalMaxSpeed;
	}
	
	if (UArcMoverInputProducerComponent* Producer = MoverComp->GetOwner()->FindComponentByClass<UArcMoverInputProducerComponent>())
	{
		Producer->OverrideInput.Reset();
		Producer->GaitOverride.Reset();
		Producer->bStopImmidietly.Reset();
	}
}

FMovementModifierBase* FArcMovementModifier_MaxVelocity::Clone() const
{
	return new FArcMovementModifier_MaxVelocity(*this);
}

void FArcMovementModifier_MaxVelocity::NetSerialize(FArchive& Ar)
{
	
}

UScriptStruct* FArcMovementModifier_MaxVelocity::GetScriptStruct() const
{
	return FArcMovementModifier_MaxVelocity::StaticStruct();  
}

FMovementModifierHandle UArcMoverFunctions::ApplyMaxVelocity(AActor* Target, float MaxVelocity, EArcMoverGaitType Gait)
{
	if (UMoverComponent* Mover = Target->FindComponentByClass<UMoverComponent>())
	{
		TSharedPtr<FArcMovementModifier_MaxVelocity> Modifier = MakeShareable(new FArcMovementModifier_MaxVelocity());
		Modifier->MaxVelocity = MaxVelocity;
		Modifier->Gait = Gait;
		FMovementModifierHandle Handle = Mover->QueueMovementModifier(Modifier);
		
		return Handle;
	}
	
	return FMovementModifierHandle(MODIFIER_INVALID_HANDLE);
}

UScriptStruct* FArcSmoothWalkingState::GetScriptStruct() const
{
	return FArcSmoothWalkingState::StaticStruct();
}

FMoverDataStructBase* FArcSmoothWalkingState::Clone() const
{
	return new FArcSmoothWalkingState(*this);
}

UArcSmoothWalkingMode::UArcSmoothWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcSmoothWalkingMode::GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity
															, const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
	Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, DesiredVelocity, DesiredFacing, CurrentFacing, InOutAngularVelocityDegrees, InOutVelocity);
	
	const FArcSmoothWalkingState* State = StartState.SyncState.SyncStateCollection.FindDataByType<FArcSmoothWalkingState>();
	
	if (const UCommonLegacyMovementSettings* MovementSettings = GetMoverComponent<UMoverComponent>()->FindSharedSettings<UCommonLegacyMovementSettings>())
	{
		InOutVelocity = InOutVelocity.GetClampedToMaxSize(MovementSettings->MaxSpeed);
	}
}
