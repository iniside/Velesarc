/**
 * This file is part of Velesarc
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

#include "Pawn/ArcMassPawnComponent.h"
#include "MoverSimulationTypes.h"
#include "Mover/ArcMoverTypes.h"

#include <atomic>

#include "ArcMassMoverInputProducerComponent.generated.h"

class UArcMassPawnExtensionComponent;
class UNavMoverComponent;
class UCurveFloat;
class IGameplayTagAssetInterface;

USTRUCT(BlueprintType)
struct FArcMassMoverRotationThresholds
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover")
	float FL = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover")
	float FR = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover")
	float BL = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover")
	float BR = 0;
};

UCLASS(ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCMASSMOVER_API UArcMassMoverInputProducerComponent
		: public UArcMassPawnComponent
		, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	TOptional<FVector> OverrideInput;

	UPROPERTY(EditAnywhere)
	TOptional<EArcMoverGaitType> GaitOverride;

	UPROPERTY(EditAnywhere)
	TOptional<EArcMoverAimModeType> AimModeOverride;

	UPROPERTY(EditAnywhere, Category = "Nav Movement")
	EMoveInputType NavMoveInputType = EMoveInputType::DirectionalIntent;

	float ControlRotationRate = 0;
	FRotator LastControlRotation = FRotator::ZeroRotator;
	FVector DirectionOfMovement = FVector::ZeroVector;
	float MovementDirectionAngle = 0;

	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FVector CachedMoveInputVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere)
	int32 RotationModeType = 0;

	UPROPERTY()
	mutable TObjectPtr<UArcMassPawnExtensionComponent> PawnExtensionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Nav Movement")
	mutable TObjectPtr<UNavMoverComponent> NavMoverComponent;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetF;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetB;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetLL;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetLR;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetRL;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetRR;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetSlide;

	UArcMassMoverInputProducerComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime
							   , enum ELevelTick TickType
							   , FActorComponentTickFunction* ThisTickFunction) override;

	virtual void ProduceInput_Implementation(int32 SimTimeMs
											 , FMoverInputCmdContext& InputCmdResult) override;

	UArcMassPawnExtensionComponent* GetPawnExtensionComponent() const;
	UNavMoverComponent* GetNavMoverComponent() const;

	void RequestJump();
	void ReleaseJump();

private:
	bool bJumpHeld = false;
	bool bJumpJustPressedPending = false;

	EArcMoverDirectionType LastMovementDirection = EArcMoverDirectionType::F;
	double LastRotationOffset = 0.0;
	FVector LastOrientationIntent = FVector::ZeroVector;

	std::atomic<int32> DebugStopProduceInputCount{0};
	FDelegateHandle DebugStopPostActorTickHandle;

	void HandleDebugStopPostActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);
	void LogDebugStopFrame();

	void ApplyMoveInput(FCharacterDefaultInputs& CharacterInputs
						, UArcMassPawnExtensionComponent* HeroComp
						, APawn* Pawn) const;

	bool ApplyNavMovement(FCharacterDefaultInputs& CharacterInputs
						  , IGameplayTagAssetInterface* GTAI
						  , TOptional<EArcMoverGaitType>& OutMassGait);

	TOptional<EArcMoverGaitType> ResolveMassGait() const;

	EArcMoverGaitType ResolveFinalGait(IGameplayTagAssetInterface* GTAI
									   , const TOptional<EArcMoverGaitType>& MassGait) const;

	bool ApplyMassFocus(FCharacterDefaultInputs& CharacterInputs
						, FArcMoverCustomInputs& CustomInputs
						, APawn* Pawn) const;

	bool ApplyAIFocus(FCharacterDefaultInputs& CharacterInputs
					  , FArcMoverCustomInputs& CustomInputs
					  , APawn* Pawn) const;

	void ApplyOrientationIntent(FCharacterDefaultInputs& CharacterInputs
								, const FArcMoverCustomInputs& CustomInputs
								, APawn* Pawn
								, bool bMassFocusSource);

	void ApplyGait(FArcMoverCustomInputs& CustomInputs
				   , IGameplayTagAssetInterface* GTAI
				   , const TOptional<EArcMoverGaitType>& MassGait) const;

	void ComputeMovementDirection(const FCharacterDefaultInputs& CharacterInputs
								  , FArcMoverCustomInputs& CustomInputs
								  , APawn* Pawn);

	void DrawDebug(const FCharacterDefaultInputs& CharacterInputs
				   , const FArcMoverCustomInputs& CustomInputs
				   , APawn* Pawn
				   , float InMovementDirectionAngle) const;


	FArcMassMoverRotationThresholds GetRotationThresholds(EArcMoverDirectionType Type) const
	{
		FArcMassMoverRotationThresholds Value;

		if (RotationModeType == 0)
		{
			switch (Type)
			{
				case EArcMoverDirectionType::F:
				case EArcMoverDirectionType::B:
					Value.FL = -60.f;
					Value.FR = 60.f;
					Value.BL = -120.f;
					Value.BR = 120.f;
					break;
				case EArcMoverDirectionType::LL:
				case EArcMoverDirectionType::LR:
				case EArcMoverDirectionType::RL:
				case EArcMoverDirectionType::RR:
					Value.FL = -40.f;
					Value.FR = 40.f;
					Value.BL = -140.f;
					Value.BR = 140.f;
					break;
			}
		}
		else if (RotationModeType == 1)
		{
			switch (Type)
			{
				case EArcMoverDirectionType::B:
					Value.FL = -120.f;
					Value.FR = 120.f;
					Value.BL = -120.f;
					Value.BR = 120.f;
					break;
				case EArcMoverDirectionType::F:
				case EArcMoverDirectionType::LL:
				case EArcMoverDirectionType::LR:
				case EArcMoverDirectionType::RL:
				case EArcMoverDirectionType::RR:
					Value.FL = -140.f;
					Value.FR = 140.f;
					Value.BL = -140.f;
					Value.BR = 140.f;
					break;
			}
		}
		else if (RotationModeType == 2)
		{
			Value.FL = -180.f;
			Value.FR = 180.f;
			Value.BL = -180.f;
			Value.BR = 180.f;
		}
		return Value;
	}
};
