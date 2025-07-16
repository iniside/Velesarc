/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcAttributesTypes.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Debug/DebugDrawComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStaticsTypes.h"
#include "ArcCharacterMovementComponent.generated.h"


UCLASS()
class ARCCORE_API UArcCharacterMovementAttributeSet : public UArcAttributeSet
{
	GENERATED_BODY()

protected:
	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_MaxVelocityMultiplier)
	FGameplayAttributeData MaxVelocityMultiplier;

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_MaxAccelerationMultiplier)
	FGameplayAttributeData MaxAccelerationMultiplier;

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_MaxDecclerationMultiplier)
	FGameplayAttributeData MaxDecclerationMultiplier;

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_MaxBrakingMultiplier)
	FGameplayAttributeData MaxBrakingMultiplier;
	
public:
	UArcCharacterMovementAttributeSet();
	
	UFUNCTION()
	void OnRep_MaxVelocityMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxAccelerationMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxDecclerationMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxBrakingMultiplier(const FGameplayAttributeData& OldValue);
	
	ARC_ATTRIBUTE_ACCESSORS(UArcCharacterMovementAttributeSet, MaxVelocityMultiplier);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcCharacterMovementAttributeSet, MaxVelocityMultiplier);
	
	ARC_ATTRIBUTE_ACCESSORS(UArcCharacterMovementAttributeSet, MaxAccelerationMultiplier);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcCharacterMovementAttributeSet, MaxAccelerationMultiplier);
	
	ARC_ATTRIBUTE_ACCESSORS(UArcCharacterMovementAttributeSet, MaxDecclerationMultiplier);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcCharacterMovementAttributeSet, MaxDecclerationMultiplier);

	ARC_ATTRIBUTE_ACCESSORS(UArcCharacterMovementAttributeSet, MaxBrakingMultiplier);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcCharacterMovementAttributeSet, MaxBrakingMultiplier);

	ARC_DEFINE_ATTRIBUTE_HANDLER(MaxVelocityMultiplier);
};


class FArcSavedMove_Character : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;
	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;
	virtual void PrepMoveFor(ACharacter* Character) override;

	float SavedMaxVelocityMultiplier = 1.f;
};

class FArcNetworkPredictionData_Client_Character : public FNetworkPredictionData_Client_Character
{
	typedef FNetworkPredictionData_Client_Character Super;
	
public:
	FArcNetworkPredictionData_Client_Character(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {};
	virtual FSavedMovePtr AllocateNewMove() override;
};

/**
 *
 */
UCLASS()
class ARCCORE_API UArcCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Attributes")
	FGameplayTag IsMovingTag;
	
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual auto GetMaxSpeed() const -> float override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;
	
public:
	mutable float MaxVelocityMultiplier = 1.f;
};

class AActor;
class ANavigationData;

UCLASS(meta=(BlueprintSpawnableComponent))
class ARCCORE_API UArcSmoothNavPathDebugComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	FNavPathSharedPtr Path;

	FNavPathSharedPtr SmoothPath;

	FNavPathSharedPtr DecimatedPath;

	FNavPathSharedPtr JumpPath;

	FVector Dir;
				
	FVector RightCross;
	FVector LeftCross;
				
	FVector EndLoc;
	FVector LeftEndLoc;
	TArray<FNavigationProjectionWork> Points;
	TArray<FNavigationProjectionWork> UpperPonts;
	int32 SelctedIdx = INDEX_NONE;
	int32 UpperIdx = INDEX_NONE;
	FPredictProjectilePathResult Result;
	
	float CurrentJumpPathTime = 0.f;
	UPROPERTY(EditAnywhere, Category = "Test")
	float MaxDistance = 40.f;

	UPROPERTY(EditAnywhere, Category = "Test")
	int32 NumPoints = 100;
	
	UPROPERTY(EditAnywhere, Category = "Test")
	float Interpolation = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Test")
	float DotThreshold = 70.f;
	
	UPROPERTY(EditAnywhere, Category = "Test")
	float MinDistance = 50.f;
	
	UPROPERTY(EditAnywhere, Category = "Test")
	int32 NumInterpolation = 2;

	UPROPERTY(EditAnywhere, Category = "Test")
	int32 SmoothWindow = 10;
	
	UPROPERTY(EditAnywhere, Category = "Test")
	FNavAgentProperties NavAgentProps;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	TObjectPtr<ANavigationData> NavData;

	UPROPERTY(EditAnywhere, Category = "Test")
	TSubclassOf<class UNavigationQueryFilter> FilterClass;

	
public:
	UArcSmoothNavPathDebugComponent(const FObjectInitializer& ObjectInitializ);

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void UpdateTest();
	void UpdateTestJump();
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

#if UE_ENABLE_DEBUG_DRAWING
	FDebugRenderSceneProxy* CreateDebugSceneProxy() override;

#endif // UE_ENABLE_DEBUG_DRAWING
};