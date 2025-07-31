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

#include "ArcGunRecoilInstance.h"

#include "ArcGunAttributeSet.h"
#include "ArcGunStateComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "GameplayEffectAggregator.h"
#include "AbilitySystem/ArcAbilitiesTypes.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Fragments/ArcItemFragment_GunStats.h"
#include "Player/ArcCorePlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Logging/StructuredLog.h"

FVector GetVectorInConeFromScreenRecoil(const FVector& Direction, float HorizontalDegrees, float VerticalDegrees)
{
	// Ensure Direction is normalized
	FVector NormalizedDir = Direction.GetSafeNormal();
    
	// Convert our screen recoil to angles in radians
	float HorizontalRadians = FMath::DegreesToRadians(HorizontalDegrees);
	float VerticalRadians = FMath::DegreesToRadians(VerticalDegrees);
    
	// Find orthogonal vectors to create our coordinate system
	FVector UpVector = FVector(0, 0, 1);
	// If Direction is nearly parallel to UpVector, use a different reference
	if (FMath::Abs(FVector::DotProduct(NormalizedDir, UpVector)) > 0.99f)
	{
		UpVector = FVector(0, 1, 0);
	}
    
	// Create orthogonal vectors
	FVector RightVector = FVector::CrossProduct(NormalizedDir, UpVector).GetSafeNormal();
	FVector NewUpVector = FVector::CrossProduct(RightVector, NormalizedDir).GetSafeNormal();
    
	// Calculate the angle offsets
	float SinVertical = FMath::Sin(VerticalRadians);
	float CosVertical = FMath::Cos(VerticalRadians);
	float SinHorizontal = FMath::Sin(HorizontalRadians);
	float CosHorizontal = FMath::Cos(HorizontalRadians);
    
	// Create the offset direction
	FVector OffsetDir = NormalizedDir * CosVertical * CosHorizontal +
						RightVector * SinHorizontal +
						NewUpVector * SinVertical;
    
	// Ensure it's normalized
	return OffsetDir.GetSafeNormal();
}


FVector FArcGunRecoilInstance_Base::GetScreenORecoilffset(const FVector& Direction) const
{
	return GetVectorInConeFromScreenRecoil(Direction, ScreenRecoil.X, ScreenRecoil.Y);
	//const float LocalV = ScreenRecoilVertical;
	//const float LocalH = ScreenRecoilHorizontal;

	//const float LocalV = ScreenRecoil.Y;
	//const float LocalH = ScreenRecoil.X;
	//
	//if (FMath::IsNearlyEqual(LocalV, 0.0f, 0.01f) || FMath::IsNearlyEqual(LocalH, 0.0f, 0.01f))
	//{
	//	//return Direction;
	//}
	//
	//// get axes we need to rotate around
	//FMatrix const DirMat = FRotationMatrix(Direction.Rotation());
	//// note the axis translation, since we want the variation to be around X
	//FVector const DirZ = DirMat.GetScaledAxis( EAxis::Z );		
	//FVector const DirY = DirMat.GetScaledAxis( EAxis::Y );
	//
	//FVector Result = Direction.RotateAngleAxis(-LocalV, DirY);
	//Result = Result.RotateAngleAxis(LocalH, DirZ);
	//// ensure it's a unit vector (might not have been passed in that way)
	//Result = Result.GetSafeNormal();
	//return Direction;
}

void FArcGunRecoilInstance_Base::CalculateCurrentVelocityLevel(const FVector2D& InRange
															   , const FVector2D& OutRange
															   , float Velocity
															   , float DeltaTime)
{
	const float NewVelocityLevel = FMath::GetMappedRangeValueClamped(InRange, OutRange, Velocity);

	CurrentVelocityLevel = FMath::FInterpConstantTo(CurrentVelocityLevel
		, NewVelocityLevel
		, DeltaTime
		, SpreadVelocityGainInterpolation);
}

void FArcGunRecoilInstance_Base::CalculateCurrentRotationLevel(const FVector2D& InRange
	, const FVector2D& OutRange
	, float RotationRate
	, float DeltaTime)
{
	const float NewRotationLevel = FMath::GetMappedRangeValueClamped(InRange, OutRange, RotationRate);
	CurrentRotationLevel = FMath::FInterpConstantTo(CurrentRotationLevel
		, NewRotationLevel
		, DeltaTime
		, RotationSpreadGainSpeed);
}

void FArcGunRecoilInstance_Base::CalculateRecoilModifires(const FArcItemData* InItem, UArcGunStateComponent* InGunState)
{
	static FGameplayTagContainer ItemTags;
	ItemTags.Reset();
	ItemTags.AppendTags(InItem->GetItemAggregatedTags());

	const float DeltaTime = InGunState->GetWorld()->GetDeltaSeconds();
	FAggregatorEvaluateParameters Params;
	Params.SourceTags = &ItemTags;
	Params.IncludePredictiveMods = true;

	const FArcItemInstance_ItemStats* ItemStats = ArcItems::FindInstance<FArcItemInstance_ItemStats>(InItem);
	TArray<FGameplayEffectAttributeCaptureSpec> HandlingMultipliers;
	
	// SPREAD
	{
		
		TArray<FGameplayEffectAttributeCaptureSpec> AccuracyBaseAttributes;

		if (ArcASC)
		{
			// TODO:: Copy and MoveTemp ?
			static FGameplayEffectAttributeCaptureSpec AccuracyCapture = UArcGunAttributeSet::GetAccuracyCaptureSpecSource(ArcASC);
			static FGameplayEffectAttributeCaptureSpec HandlingCapture = UArcGunAttributeSet::GetHandlingCaptureSpecSource(ArcASC);
		
			HandlingMultipliers.Add(HandlingCapture);
			AccuracyBaseAttributes.Add(AccuracyCapture);
		}
	
		
		const float AccuracyStat = ItemStats ? ItemStats->GetStatValue(UArcGunAttributeSet::GetAccuracyAttribute()) : 1.f;

		const float SpreadAttributeFinalValue = ArcAbility::CalculateAttributeLog(Params, AccuracyStat, AccuracyBaseAttributes, HandlingMultipliers);

		UCharacterMovementComponent* CMC = CharacterOwner->GetCharacterMovement();
		const float Velocity = CMC->Velocity.Size();
		const float Acceleration = CMC->GetCurrentAcceleration().Size();
	
		bIsAiming = ArcASC->HasMatchingGameplayTag(AimTag);
		const float TimeSinceFired = InGunState->GetWorld()->TimeSince(InGunState->GetLastShootTime());
	
		if (Acceleration > 0)
		{
			RotationSpreadGainSpeed = InItem->GetValue(ArcGunStats::GetSpreadRotationGainInterpolationData());
			SpreadVelocityGainInterpolation = InItem->GetValue(ArcGunStats::GetSpreadVelocityGainInterpolationData());	
		}
		else
		{
			RotationSpreadGainSpeed = InItem->GetValue(ArcGunStats::GetSpreadRotationGainInterpolationData());
			SpreadVelocityGainInterpolation = InItem->GetValue(ArcGunStats::GetSpreadVelocityRecoveryInterpolationData());
		}
	
		const FVector2D OutRange(0, 1);
		const FVector2D InRange(80, 200);
		CalculateCurrentVelocityLevel(InRange, OutRange, Velocity, DeltaTime);

		const FVector2D InRotationRange(5, 40);
		CalculateCurrentRotationLevel(InRotationRange, OutRange, HighestAxisRotation, DeltaTime);

		SpreadHeatRecoveryDelay = InItem->GetValue(ArcGunStats::GetSpreadHeatRecoveryDelayData());

		const float SpreadRecoveryAimMultiplier = bIsAiming ? InItem->GetValue(ArcGunStats::GetSpreadRecoveryAimMultiplierData()) : 1;
		SpreadRecoveryMultiplier = SpreadAttributeFinalValue / SpreadRecoveryAimMultiplier;

		// TODO:: This should also incorporate SpreadRecoveryAimMultiplier
		SpreadHeatGainMultiplier = 1 / SpreadAttributeFinalValue;

		const float SpreadMovementAdditiveCurveData = bIsAiming ? 
					(InItem->GetValueWithLevel(ArcGunStats::GetAimSpreadMovementAdditiveCurveData(), CurrentVelocityLevel) / SpreadAttributeFinalValue)
					: (InItem->GetValueWithLevel(ArcGunStats::GetSpreadMovementAdditiveCurveData(), CurrentVelocityLevel) / SpreadAttributeFinalValue);

		RotationSpreadAdditive = InItem->GetValueWithLevel(ArcGunStats::GetSpreadRotationAdditiveCurveData(), CurrentRotationLevel);
		VelocitySpreadAdditive = SpreadMovementAdditiveCurveData;

		const float SpreadAimMultiplier = bIsAiming ? InItem->GetValue(ArcGunStats::GetSpreadAimMultiplierData()) : 1;
		SpreadAngleMultiplier = SpreadAimMultiplier * SpreadAttributeFinalValue;//FMath::Max<float>(1, (Data.SpreadRotationMultiplier) / SpreadAttributeFinalValue) * SpreadAimMultiplier;
	}
	
	// RECOIL
	{
		TArray<FGameplayEffectAttributeCaptureSpec> StabilityBaseAttributes;
		if (ArcASC)
		{
			FGameplayEffectAttributeCaptureSpec StabilityCapture = UArcGunAttributeSet::GetStabilityCaptureSpecSource(ArcASC);
			StabilityBaseAttributes.Add(StabilityCapture);
		}
	
		const float StabilityStat = ItemStats ? ItemStats->GetStatValue(UArcGunAttributeSet::GetStabilityAttribute()) : 1.f;

		const float RecoilAttributeFinalValue = ArcAbility::CalculateAttributeLog(Params, StabilityStat, StabilityBaseAttributes, HandlingMultipliers);
	
		const float RecoilAimMultiplier = bIsAiming ? InItem->GetValue(ArcGunStats::GetRecoilAimMultiplierData()) : 1;

		RecoilHeatRecoveryDelay = InItem->GetValue(ArcGunStats::GetRecoilHeatRecoveryDelayData());
		RecoilYawMultiplier = RecoilAimMultiplier / RecoilAttributeFinalValue;
		RecoilPitchMultiplier = RecoilAimMultiplier / RecoilAttributeFinalValue;
	
		RecoilHeatGainMultiplier = 1 / RecoilAttributeFinalValue;
	
		ScreenRecoilInterpolationSpeed = ArcGunStats::GetScreenRecoilGainInterpolationSpeedValue(InItem);
		ScreenRecoilRecoveryInterpolationSpeed = ArcGunStats::GetScreenRecoilRecoveryInterpolationSpeedValue(InItem);
	}

	// SWAY
	{
		
	}
}

void FArcGunRecoilInstance_Base::Deinitialize()
{
}

void FArcGunRecoilInstance_Base::StartRecoil(UArcGunStateComponent* InGunState)
{
	if (InGunState->GetPlayerController())
	{
		CharacterOwner = InGunState->GetPlayerController()->GetPawn<ACharacter>();	
	}
	
	if (CharacterOwner == nullptr)
	{
		CharacterOwner = InGunState->GetOwner<ACharacter>();
		if (CharacterOwner == nullptr)
		{
			APlayerState* PS = InGunState->GetOwner<APlayerState>();
			CharacterOwner = PS->GetPawn<ACharacter>();
		}
	}
	
	if (ArcPC == nullptr)
	{
		ArcPC = CharacterOwner->GetController<AArcCorePlayerController>();	
	}
	
	if (!ArcPC)
	{
		return;
	}
	

	if (AxisValueBindings.IsEmpty())
	{
		if (UEnhancedInputComponent* InputComp = Cast<UEnhancedInputComponent>(ArcPC->InputComponent))
		{
			if (UInputAction* AxisAction = InGunState->GetAxisInputAction())
			{
				FEnhancedInputActionValueBinding* AxisValueBinding = &InputComp->BindActionValue(AxisAction);
				AxisValueBindings.Add(AxisValueBinding);
			}
		}
	}
	
	StartCameraLocation = FVector::ZeroVector;
	EndCameraLocation = FVector::ZeroVector;
	ENetMode NM = InGunState->GetOwner()->GetNetMode();

	if (NM == ENetMode::NM_DedicatedServer)
	{
		return;
	}



	

	ArcASC = InGunState->GetArcASC();

	CalculateRecoilModifires(InGunState->GetSelectedGun().GunItemPtr, InGunState);

	StartYInputValue = ArcPC->InputValue.Y;
		
	ScreenRecoil = FVector2D::ZeroVector;
	TargetScreenRecoil = FVector2D::ZeroVector;
	
	FRotator Unused;
	CharacterOwner->GetActorEyesViewPoint(StartCameraLocation, Unused);
	
	RecoveredPitch = 0;
	bBreakRecovery = true;
;
	bIsFiring = true;
	
	const ArcGunStats* StatsFragment = InGunState->GetSelectedGun().GunItemPtr->GetItemDefinition()->GetScalableFloatFragment<ArcGunStats>();
	
	ShotsFired        = 0;
	BiasValue         = 0.f;
	BiasAccelPerShot  = 1.f / FMath::Max(1, StatsFragment->GetHeatToFullBiasDirect(1));
	AccumulatedPhase = 0;
	LocalSpaceRecoilAccumulatedPhase = 0;
	
	// Decide initial drift
	if (StatsFragment->GetInitialBiasDirectionDirect(1)  < 0)
	{
		TargetBiasDir = -1;
	}
	else if (StatsFragment->GetInitialBiasDirectionDirect(1)  > 0)
	{
		TargetBiasDir = +1;
	}
	else
	{
		TargetBiasDir = FMath::RandBool() ? +1 : -1;
	}
	
	const float DeltaTime = InGunState->PrimaryComponentTick.TickInterval;
	UpdateRecoil(DeltaTime, InGunState);
}

float SmoothRecoveredInputValue(float InputValue, float SmallValueThreshold, float ScalingFactor)
{
	// If the value is below the threshold, keep it mostly intact
	if (FMath::Abs(InputValue) <= SmallValueThreshold)
	{
		return InputValue;
	}
	// For larger values, apply a smoothing function
	else
	{
		// Calculate how much the value exceeds the threshold
		float ExcessValue = FMath::Abs(InputValue) - SmallValueThreshold;
        
		// Apply a logarithmic or square root scaling to the excess portion
		float ScaledExcess = SmallValueThreshold + (FMath::Loge(1 + ExcessValue) * ScalingFactor);

        
		// Preserve the original sign
		return (InputValue >= 0) ? ScaledExcess : -ScaledExcess;
	}
}

void FArcGunRecoilInstance_Base::StopRecoil(UArcGunStateComponent* InGunState)
{
	FRotator Unused;
	CharacterOwner->GetActorEyesViewPoint(EndCameraLocation, Unused);
	
	TargetScreenRecoil = FVector2D::ZeroVector;
	
	RecoveredPitch = 0;
	bBreakRecovery = false;
	bIsFiring = false;


	ArcPC->ResetPitchModifier();
	ArcPC->ResetYawModifier();
	
	TimeAccumulator = 0.f;
	ShotsFired   = 0;
	BiasValue    = 0.f;
	TargetBiasDir = 0;
	AccumulatedPhase = 0;
	
	ArcPC->ControlRotationOffset.Pitch = 0;
	ArcPC->ControlRotationOffset.Yaw = 0;

	EndYInputValue = ArcPC->InputValue.Y;

	const float StartSign = FMath::Sign(StartYInputValue);
	const float EndSign = FMath::Sign(EndYInputValue);

	ArcPC->ClampValue = StartYInputValue;
	
	if (StartSign != EndSign)
	{
		if (EndYInputValue > StartYInputValue)
		{
			RecoveryDirection = -1;
			RecoveredInputValue = FMath::Abs((EndYInputValue *EndSign + StartYInputValue*StartSign));
		}
		else
		{
			RecoveryDirection = -1;
			RecoveredInputValue = FMath::Abs(StartYInputValue + EndYInputValue);
		}	
	}
	else 
	{
		if (EndYInputValue > StartYInputValue)
		{
			RecoveryDirection = -1;
			RecoveredInputValue = (EndYInputValue - StartYInputValue);
		}
		else
		{
			RecoveryDirection = -1;
			RecoveredInputValue = (StartYInputValue - EndYInputValue);
		}
	}


}

float GetPatternBasedRecoil(float baseMin, float baseMax, int shotCount, float biasStrength, float timeSinceFiring)
{
	// Pattern cycle length - how many shots before pattern repeats or changes direction
	const int patternCycleLength = 3;
    
	// Define pattern direction based on shot count
	float direction = 1.0f;
    
	// Create patterns like: first 3 shots go right, next 3 go left
	if ((shotCount / patternCycleLength) % 2 == 0)
	{
		direction = 1.0f; // Right/Up
	}
	else
	{
		direction = -1.0f; // Left/Down
	}
    
	// Increase magnitude based on consecutive shots in the same direction
	float consecutiveBonus = FMath::Max((shotCount % patternCycleLength) * (biasStrength * timeSinceFiring), 0.5f);
    
	// Base random value (this still has randomness, but will follow the pattern)
	float randomComponent = FMath::FRandRange(baseMin, baseMax);
    
	// More shots = more predictable (less random component)
	float patternInfluence = FMath::Min(shotCount * 0.1f, 0.8f);
	float randomInfluence = 1.0f - patternInfluence;
    
	// Blend between random and pattern-based recoil
	float baseValue = baseMin + (baseMax - baseMin) * 0.5f; // Middle of range
	float patternValue = baseValue * direction * (1.0f + consecutiveBonus);
    
	return (randomComponent * randomInfluence) + (patternValue * patternInfluence);

}

FVector2D FArcGunRecoilInstance_Base::GenerateSemiRandomSpreadPattern(
    float BaseSpread,
    float HorizontalBias,       // Range: -1.0 (left) to 1.0 (right)
    float VerticalBias,         // Range: -1.0 (down) to 1.0 (up)
    float HeatScale,
    float OscillationPeriod,     // Chance to temporarily move in opposite direction (0-1)
    float OscillationStrength,   // How strong the oscillation movement is (0-1)
    float OscillationDownwardChance,
    float HorizontalTightness,   // Controls horizontal spread tightness (0-1)
    float MaxAccumulatedValue,  // Maximum spread to allow when accumulated
    int32 ShotCount)
{
    // Clamp parameters to valid ranges
    HorizontalBias = FMath::Clamp(HorizontalBias, -1.0f, 1.0f);
    VerticalBias = FMath::Clamp(VerticalBias, -1.0f, 1.0f);
    OscillationStrength = FMath::Clamp(OscillationStrength, 0.0f, 1.0f);
    HorizontalTightness = FMath::Clamp(HorizontalTightness, 0.0f, 1.0f);
    
    // Calculate small base value suitable for accumulation
    float ScaledBaseSpread = BaseSpread;

	UE_LOG(LogTemp, Log, TEXT("1 ScaledBaseSpread %f"), ScaledBaseSpread);
	
    // Apply heat scaling if needed (avoid excessive growth)
    float HeatFactor = 1.0f + (RecoilHeat * HeatScale);
    ScaledBaseSpread *= HeatFactor;

	UE_LOG(LogTemp, Log, TEXT("2 ScaledBaseSpread %f"), ScaledBaseSpread);
	
    // Calculate directional vector for main bias
    FVector2D DirectionalBias(HorizontalBias, VerticalBias);
    if (!DirectionalBias.IsNearlyZero())
    {
        DirectionalBias.Normalize();
    }
	else
	{
        // Default to slight upward bias if no direction specified
        DirectionalBias = FVector2D(0.0f, 0.5f);
    }
	
	float oscValue = 1;;
	if (OscillationStrength > KINDA_SMALL_NUMBER && OscillationPeriod > 0)
	{
		// Increment phase each time this function is called
		// This ensures oscillation continues even when RecoilHeat is clamped
		LocalSpaceRecoilAccumulatedPhase += TWO_PI / OscillationPeriod;
        
		// Ensure phase wraps around so it doesn't grow infinitely
		if (LocalSpaceRecoilAccumulatedPhase > TWO_PI)
		{
			LocalSpaceRecoilAccumulatedPhase -= TWO_PI;
		}
		// Get the base sine wave value (-1 to 1)
		float sineValue = FMath::Sin(LocalSpaceRecoilAccumulatedPhase);
        
		// Process the sine wave based on the DownwardRecoilFactor
		
		if (OscillationDownwardChance <= 0.0f)
		{
			// No downward recoil at all - only positive values
			oscValue = FMath::Max(0.0f, sineValue) * 2.0f; // Normalize range
		}
		else
		{
			// Generate a random value between 0 and 1
			float randomChance = FMath::FRandRange(0.f, 1.f);
            
			// If the random value is less than our factor, allow downward recoil
			const bool allowDownwardRecoil = (randomChance < OscillationDownwardChance);
			if (sineValue < 0.0f && !allowDownwardRecoil)
			{
				// If sine is negative but downward recoil isn't allowed for this cycle,
				// force it to zero (no recoil) instead of allowing downward motion
				oscValue = 0.0f;
			}
			else
			{
				// Otherwise use the actual sine value
				oscValue = sineValue;
			}
		}
        
		ScaledBaseSpread *= 1.f + OscillationStrength * oscValue;

	}
	
    // --- Generate controlled random component with appropriate tightness ---
    // Horizontal spread with tightness control
    float HTightnessPower = 1.0f + (HorizontalTightness * 3.0f); // Range 1-4
    float RawHRand = FMath::FRandRange(-1.0f, 1.0f) + 0.5;;
    float TightHRand = FMath::Sign(RawHRand) * FMath::Pow(FMath::Abs(RawHRand), HTightnessPower);
    
    // Vertical spread (can be looser than horizontal)
    float VTightnessPower = 1.0f + ((1.0f - HorizontalTightness) * 2.0f); // Inverse of horizontal tightness
    float RawVRand = FMath::FRandRange(-1.0f, 1.0f);
    float TightVRand = FMath::Sign(RawVRand) * FMath::Pow(FMath::Abs(RawVRand), VTightnessPower);
    
    // --- Apply sequence-based pattern variation ---
    // This creates slight predictable patterns over sequences of shots
    float SequenceFactor = FMath::Sin(ShotCount * 0.2f) * 0.4f;
    float SequenceX = FMath::Cos(ShotCount * 0.17f) * SequenceFactor;
    float SequenceY = FMath::Sin(ShotCount * 0.23f) * SequenceFactor;
    
    // --- Combine components ---
    // Directional bias (main direction or oscillated)
    //float DirectionalX = DirectionalBias.X * DirectionalStrength;
    //float DirectionalY = DirectionalBias.Y * DirectionalStrength;
    
    // Random component (controlled by tightness)
    float RandomWeight = (1.0f - HeatScale) * 0.8f;
    float RandomX = TightHRand * RandomWeight;
    float RandomY = TightVRand * RandomWeight;// * oscValue;
	RandomY *= 1.f + OscillationStrength * oscValue;
	
    // Sequence pattern component
    //float SequenceWeight = (1.0f - DirectionalStrength) * 0.2f;
    
    // Combine all
    float FinalX = RandomX + (SequenceX);// * SequenceWeight);
    float FinalY = RandomY + (SequenceY);// * SequenceWeight);

	UE_LOG(LogTemp, Log, TEXT("3 RandomY %f SequenceY %f oscValue %f SequenceFactor %f RandomWeight %f"), RandomY, SequenceY, oscValue, SequenceFactor, RandomWeight);
    // Create result vector
    FVector2D Result(FinalX, FinalY);
    
    // Normalize and apply appropriate scaling for accumulation
    if (!Result.IsNearlyZero())
    {
        Result.Normalize();
        
        // Apply strength curve based on shot count to simulate progressive recoil
        float ShotProgression = FMath::Min(1.0f, ShotCount / 10.0f); // Saturates after 10 shots
        float ProgressionFactor = 1.0f + (ShotProgression * 0.5f);   // Up to 50% stronger
        
        // Apply scaling
        Result *= ScaledBaseSpread * ProgressionFactor;
    }
    
    return Result;
}



float FArcGunRecoilInstance_Base::ComputeRecoilStrength(float InRecoilHeat,
							float PeakStrength,     // Increased from 1.6f to allow higher maximum
							float RampUpShots,    // Increased to make ramp-up more gradual
							float GrowthRate,    // New parameter: controls how quickly recoil grows
							float OscAmplitude,    // Reduced from 0.15f for less randomness
							float OscPeriod,        // Increased for more predictable oscillation
							float NoiseJitter,
							float DownwardRecoilFactor)    // Reduced from 0.05f for less randomness
{
	// 1. Normalize heat for consistent calculation
	const float NormalizedHeat = (RecoilHeat - 0) / (10 - 0);
    
	// 2. Basic ramp-up with more aggressive growth curve
	// Using pow to create exponential growth instead of linear
	const float tRamp = FMath::Clamp(NormalizedHeat, 0.f, 1.f);
	float strength = FMath::Lerp(1.f, PeakStrength, FMath::Pow(tRamp, 0.7f));

	UE_LOG(LogTemp, Log, TEXT("1 Recoil Heat %f NormalizedHeat %f tRamp %f StrengthMul %f"), RecoilHeat, NormalizedHeat, tRamp, strength);
	
	// 3. Additional growth factor for sustained fire
	// This replaces the old decay logic - now it gets stronger over time
	if (RecoilHeat > RampUpShots && GrowthRate > KINDA_SMALL_NUMBER)
	{
		const float shotsPastRamp = RecoilHeat - RampUpShots;
		// Apply diminishing returns on growth to prevent getting too extreme
		strength *= (1.f + GrowthRate * FMath::Sqrt(shotsPastRamp));
	}

	UE_LOG(LogTemp, Log, TEXT("2 Recoil Heat %f NormalizedHeat %f tRamp %f StrengthMul %f"), RecoilHeat, NormalizedHeat, tRamp, strength);
	// 4. Reduced oscillation (more predictable pattern)
	
	if (OscAmplitude > KINDA_SMALL_NUMBER && OscPeriod > 0)
	{
		// Increment phase each time this function is called
		// This ensures oscillation continues even when RecoilHeat is clamped
		AccumulatedPhase += TWO_PI / OscPeriod;
        
		// Ensure phase wraps around so it doesn't grow infinitely
		if (AccumulatedPhase > TWO_PI)
		{
			AccumulatedPhase -= TWO_PI;
		}
		// Get the base sine wave value (-1 to 1)
		float sineValue = FMath::Sin(AccumulatedPhase);
        
		// Process the sine wave based on the DownwardRecoilFactor
		float oscValue;
		if (DownwardRecoilFactor <= 0.0f)
		{
			// No downward recoil at all - only positive values
			oscValue = FMath::Max(0.0f, sineValue) * 2.0f; // Normalize range
		}
		else
		{
			// Generate a random value between 0 and 1
			float randomChance = FMath::FRandRange(0.f, 1.f);
            
			// If the random value is less than our factor, allow downward recoil
			const bool allowDownwardRecoil = (randomChance < DownwardRecoilFactor);
			if (sineValue < 0.0f && !allowDownwardRecoil)
			{
				// If sine is negative but downward recoil isn't allowed for this cycle,
				// force it to zero (no recoil) instead of allowing downward motion
				oscValue = 0.0f;
			}
			else
			{
				// Otherwise use the actual sine value
				oscValue = sineValue;
			}
		}
        
		strength *= 1.f + OscAmplitude * oscValue;

	}

	
	// 5. Minimal random noise - just enough to not feel robotic
	if (NoiseJitter > KINDA_SMALL_NUMBER)
	{
		strength *= FMath::FRandRange(1.f - NoiseJitter, 1.f + NoiseJitter);
	}

	UE_LOG(LogTemp, Log, TEXT("4 Recoil Heat %f NormalizedHeat %f tRamp %f StrengthMul %f"), RecoilHeat, NormalizedHeat, tRamp, strength);
	return strength;
}
void FArcGunRecoilInstance_Base::OnShootFired(UArcGunStateComponent* InGunState)
{
	for (FEnhancedInputActionValueBinding* AxisValueBinding : AxisValueBindings)
	{
		if (!AxisValueBinding)
		{
			continue;
		}

		const FVector2d Value = AxisValueBinding->GetValue().Get<FVector2D>();
		const double ValueSquaredLength = Value.SquaredLength();
		
		HighestAxisRotation = ValueSquaredLength;
	}
	
	const ArcGunStats* StatsFragment = InGunState->GetSelectedGun().GunItemPtr->GetItemDefinition()->GetScalableFloatFragment<ArcGunStats>();

	const float Level = 1;
	
	CalculateRecoilModifires(InGunState->GetSelectedGun().GunItemPtr, InGunState);
	
	LastFireTime = InGunState->GetWorld()->GetTimeSeconds();
	
	// Sample the heat up curve
	const float SpreadHeatPerShot = StatsFragment->HeatToHeatPerShotCurve->GetFloatValue(SpreadHeat) * HeatPerShotMultiplier;
	const float RecoilHeatPerShot = StatsFragment->HeatToHeatPerShotCurve->GetFloatValue(RecoilHeat) * HeatPerShotMultiplier;
	
	SpreadHeat = ClampSpreadHeat(StatsFragment, SpreadHeat + (SpreadHeatPerShot * SpreadHeatGainMultiplier));
	RecoilHeat = ClampRecoilHeat(StatsFragment, RecoilHeat + (RecoilHeatPerShot * RecoilHeatGainMultiplier));

#if 0
	const FVector2D LocalSpaceRecoilNew = GenerateSemiRandomSpreadPattern(
		StatsFragment->GetLocalSpaceRecoilBaseDirect(Level)
		, StatsFragment->GetLocalSpaceRecoilHorizontalBiasDirect(Level) // Range: -1.0 (left) to 1.0 (right)
		, StatsFragment->GetLocalSpaceRecoilVerticalBiasDirect(Level) // Range: -1.0 (down) to 1.0 (up)
		, StatsFragment->GetLocalSpaceRecoilHeatScaleDirect(Level) // How strongly to follow the main bias (0-1)
		, StatsFragment->GetLocalSpaceRecoilOscillationPeriodDirect(Level) // Chance to temporarily move in opposite direction (0-1)
		, StatsFragment->GetLocalSpaceRecoilOscillationStrengthDirect(Level) // How strong the oscillation movement is (0-1)
		, 0//StatsFragment->GetLocalSpaceRecoilOscillationDownwardChanceDirect(Level) // How likely to oscillate downward (0-1)
		, StatsFragment->GetLocalSpaceRecoilHorizontalTightnessDirect(Level) // Controls horizontal spread tightness (0-1)
		, ShotsFired
		);
	
	UE_LOG(LogTemp, Log, TEXT("LocalSpaceRecoilNew %s"), *LocalSpaceRecoilNew.ToString());
	TargetScreenRecoil += LocalSpaceRecoilNew;
	TargetScreenRecoil.X = FMath::Clamp(TargetScreenRecoil.X, StatsFragment->GetLocalSpaceRecoilHorizontalMinValueDirect(Level), StatsFragment->GetLocalSpaceRecoilHorizontalMaxValueDirect(Level));
	TargetScreenRecoil.Y = FMath::Clamp(TargetScreenRecoil.Y, StatsFragment->GetLocalSpaceRecoilVerticalMinValueDirect(Level), StatsFragment->GetLocalSpaceRecoilVerticalMaxValueDirect(Level));
#endif	
	bBreakRecovery = true;

}

void FArcGunRecoilInstance_Base::ComputeSpreadHeatRange(
	const ArcGunStats* WeaponStats
	, float& MinHeat
	, float& MaxHeat) const
{
	float Min1;
	float Max1;
	WeaponStats->HeatToHeatPerShotCurve->GetTimeRange(/*out*/ Min1, /*out*/ Max1);

	float Min2;
	float Max2;
	WeaponStats->HeatToSpreadRecoveryPerSecondCurve->GetTimeRange(/*out*/ Min2, /*out*/ Max2);

	float Min3;
	float Max3;
	WeaponStats->HeatToSpreadCurve->GetTimeRange(/*out*/ Min3, /*out*/ Max3);

	MinHeat = FMath::Min(FMath::Min(Min1, Min2), Min3);
	MaxHeat = FMath::Max(FMath::Max(Max1, Max2), Max3);
}

float FArcGunRecoilInstance_Base::ClampSpreadHeat(
	const ArcGunStats* WeaponStats
	, float NewHeat) const
{
	float MinHeat;
	float MaxHeat;
	ComputeSpreadHeatRange(WeaponStats, /*out*/ MinHeat, /*out*/ MaxHeat);

	return FMath::Clamp(NewHeat, MinHeat, MaxHeat);
}

void FArcGunRecoilInstance_Base::ComputeRecoilHeatRange(const ArcGunStats* WeaponStats
	, float& MinHeat
	, float& MaxHeat) const
{
	float Min1;
	float Max1;
	WeaponStats->HeatToHeatPerShotCurve->GetTimeRange(/*out*/ Min1, /*out*/ Max1);

	float Min2;
	float Max2;
	WeaponStats->HeatToRecoilRecoveryPerSecond->GetTimeRange(/*out*/ Min2, /*out*/ Max2);

	float Min3;
	float Max3;
	WeaponStats->HeatToRecoilCurve->GetTimeRange(/*out*/ Min3, /*out*/ Max3);

	MinHeat = FMath::Min(FMath::Min(Min1, Min2), Min3);
	MaxHeat = FMath::Max(FMath::Max(Max1, Max2), Max3);
}

float FArcGunRecoilInstance_Base::ClampRecoilHeat(const ArcGunStats* WeaponStats
	, float NewHeat) const
{
	float MinHeat;
	float MaxHeat;
	ComputeRecoilHeatRange(WeaponStats, /*out*/ MinHeat, /*out*/ MaxHeat);

	//UE_LOG(LogTemp, Log, TEXT("MinRecoilHeat %f MaxHeat %f"), MinHeat, MaxHeat)
	return FMath::Clamp(NewHeat, MinHeat, MaxHeat);
}

void FArcGunRecoilInstance_Base::HandlePreProcessRotation(float Value)
{
	if(bIsFiring == false && Value != 0)
	{
		bBreakRecovery = true;
	}
}

namespace Arcx
{
	void DrawText(AHUD* HUD
				, class UCanvas* Canvas
				, const FString& Text
				, float Offset)
	{
		Canvas->DrawText(
			GEngine->GetMediumFont(),
			Text,
			100,
			Offset,
			1.2f,
			1.2f
	);
	}
}
void FArcGunRecoilInstance_Base::DrawDebugHUD(AHUD* HUD
	, class UCanvas* Canvas
	, const class FDebugDisplayInfo& DebugDisplay
	, float& YL
	, float& YPos) const
{
	Canvas->SetDrawColor(FColor::Red);
	
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("ScreenRecoilVertical: {0}"), { ScreenRecoil.Y }), YPos + 100);
	
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("ScreenRecoilHorizontal: {0}"), { ScreenRecoil.X }), YPos + 120);
	
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("SpreadHeat: {0}"), { SpreadHeat }), YPos + 160);
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("RecoilHeat: {0}"), { RecoilHeat }), YPos + 180);
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("CurrentSpreadAngle: {0}"), { CurrentSpreadAngle }), YPos + 200);
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("RecoveredPitch: {0}"), { RecoveredPitch }), YPos + 220);
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("LastFireTime: {0}"), { LastFireTime }), YPos + 240);
	
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("RotationSpreadGainSpeed: {0}"), { RotationSpreadGainSpeed }), YPos + 260);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("SpreadVelocityGainInterpolation: {0}"), { SpreadVelocityGainInterpolation }), YPos + 280);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("HeatPerShotMultiplier: {0}"), { HeatPerShotMultiplier }), YPos + 300);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("SpreadHeatGainMultiplier: {0}"), { SpreadHeatGainMultiplier }), YPos + 320);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("SpreadHeatRecoveryDelay: {0}"), { SpreadHeatRecoveryDelay }), YPos + 340);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("VelocitySpreadAdditive: {0}"), { VelocitySpreadAdditive }), YPos + 360);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("RotationSpreadAdditive: {0}"), { RotationSpreadAdditive }), YPos + 380);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("SpreadAngleMultiplier: {0}"), { SpreadAngleMultiplier }), YPos + 400);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("SpreadRecoveryMultiplier: {0}"), { SpreadRecoveryMultiplier }), YPos + 420);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("RecoilHeatRecoveryDelay: {0}"), { RecoilHeatRecoveryDelay }), YPos + 460);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("RecoilRecoveryMultiplier: {0}"), { RecoilRecoveryMultiplier }), YPos + 480);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("RecoilHeatGainMultiplier: {0}"), { RecoilHeatGainMultiplier }), YPos + 500);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("RecoilPitchMultiplier: {0}"), { RecoilPitchMultiplier }), YPos + 520);
		
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("RecoilYawMultiplier: {0}"), { RecoilYawMultiplier }), YPos + 560);
	
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("TargetScreenRecoilVertical: {0}"), { TargetScreenRecoil.Y }), YPos + 620);

	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("TargetScreenRecoilHorizontal: {0}"), { TargetScreenRecoil.X }), YPos + 640);
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("SwayY: {0}"), { TargetSway.Y }), YPos + 660);
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("SwayX: {0}"), { TargetSway.X }), YPos + 680);
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("CurrentHorizontalSway: {0}"), { CurrentSway.X }), YPos + 700);
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("CurrentVerticalSway: {0}"), { CurrentSway.Y }), YPos + 720);
	Arcx::DrawText(HUD, Canvas, FString::Format(TEXT("CurrentRotationLevel: {0}"), { CurrentRotationLevel }), YPos + 740);
}

FVector2D GenerateSway(
    float Radius,
    float HorizontalBias = 0.0f,
    float VerticalBias = 0.0f,
    float RandomnessAmount = 0.5f,
    float TimeOffset = 0.0f)
{
    // Ensure biases are in range [-1, 1]
    HorizontalBias = FMath::Clamp(HorizontalBias, -1.0f, 1.0f);
    VerticalBias = FMath::Clamp(VerticalBias, -1.0f, 1.0f);
    RandomnessAmount = FMath::Clamp(RandomnessAmount, 0.0f, 1.0f);
    
    // Calculate deterministic component using sine waves with different frequencies
    // This creates a smooth, semi-predictable movement pattern
    const float TimeScale = 2.0f * PI;
    const float PhaseX = TimeOffset * TimeScale * 0.37f; // Different frequencies for X and Y
    const float PhaseY = TimeOffset * TimeScale * 0.53f; // Using prime number ratios for less repetitive pattern
    
    // Base deterministic position (circular pattern)
    float DeterministicX = FMath::Sin(PhaseX);
    float DeterministicY = FMath::Cos(PhaseY);
    
    // Apply bias to the deterministic component
    DeterministicX = FMath::Lerp(DeterministicX, HorizontalBias, FMath::Abs(HorizontalBias));
    DeterministicY = FMath::Lerp(DeterministicY, VerticalBias, FMath::Abs(VerticalBias));
    
    // Generate random component
    float RandomX = FMath::RandRange(-1.0f, 1.0f);
    float RandomY = FMath::RandRange(-1.0f, 1.0f);
    
    // Normalize random vector to prevent bias towards corners
    float RandomLength = FMath::Sqrt(RandomX * RandomX + RandomY * RandomY);
    if (RandomLength > SMALL_NUMBER) // Prevent division by zero
    {
        RandomX /= RandomLength;
        RandomY /= RandomLength;
    }
    
    // Blend deterministic and random components based on RandomnessAmount
    float FinalX = FMath::Lerp(DeterministicX, RandomX, RandomnessAmount);
    float FinalY = FMath::Lerp(DeterministicY, RandomY, RandomnessAmount);
    
    // Normalize and scale by radius
    float Length = FMath::Sqrt(FinalX * FinalX + FinalY * FinalY);
    if (Length > SMALL_NUMBER) // Prevent division by zero
    {
        FinalX = (FinalX / Length) * Radius;
        FinalY = (FinalY / Length) * Radius;
    }
    
    return FVector2D(FinalX, FinalY);
}

void FArcGunRecoilInstance_Base::UpdateRecoil(float DeltaTime, UArcGunStateComponent* InGunState)
{
	const FArcSelectedGun& SelectedGun = InGunState->GetSelectedGun();
	FArcGunInstanceBase* SelectedGunInstance = InGunState->GetGunInstance<FArcGunInstanceBase>();
	
	if (SelectedGun.IsValid() == false)
	{
		return;
	}

	if (!ArcPC || !ArcPC->GetPawn())
	{
		if (CharacterOwner == nullptr)
		{
			CharacterOwner = InGunState->GetOwner<ACharacter>();
			if (CharacterOwner == nullptr)
			{
				APlayerState* PS = InGunState->GetOwner<APlayerState>();
				CharacterOwner = PS->GetPawn<ACharacter>();
			}
		}

		if (ArcPC == nullptr)
		{
			ArcPC = CharacterOwner->GetController<AArcCorePlayerController>();	
		}

		ArcASC = InGunState->GetArcASC();
		
		if (!CharacterOwner || !ArcPC)
		{
			return;
		}
	}
	
	for (FEnhancedInputActionValueBinding* AxisValueBinding : AxisValueBindings)
	{
		if (!AxisValueBinding)
		{
			continue;
		}

		const FVector2d Value = AxisValueBinding->GetValue().Get<FVector2D>();
		const double ValueSquaredLength = Value.SquaredLength();
		
		HighestAxisRotation = ValueSquaredLength;
	}
	
	CalculateRecoilModifires(SelectedGun.GunItemPtr, InGunState);

	const ArcGunStats* WeaponStats = SelectedGun.GunItemPtr->GetItemDefinition()->GetScalableFloatFragment<ArcGunStats>();

	const float Speed = CharacterOwner->GetVelocity().Size();
	float NormalizedVelocity = FMath::GetMappedRangeValueClamped(WeaponStats->VelocitySwayVelocityRange, FVector2D(0, 1), Speed);

	SwayRadius = WeaponStats->GetBaseSwayRadiusDirect(1);
	const float SwayAimMultiplier = WeaponStats->GetBaseSwayAimMultiplierDirect(1);
	
	float SwayInterpolationRate = WeaponStats->GetBaseSwayInterpolationRateDirect(1);
	
	const float VelocitySwayRadiusMultiplier = ArcGunStats::GetVelocitySwayMultiplierCurveValue(WeaponStats, NormalizedVelocity);
	const float VelocitySwayInterpolationMultiplier = ArcGunStats::GetVelocitySwayInterpolationMultiplierCurveValue(WeaponStats, NormalizedVelocity);

	if (bIsAiming)
	{
		SwayRadius *= SwayAimMultiplier;
		SwayInterpolationRate *= SwayAimMultiplier;
	}
	
	if (!bIsFiring)
	{
		float TimeOffset = InGunState->GetWorld()->GetTimeSeconds(); // Or use a custom timer
		const float CurrentSwaySize = CurrentSway.Size();
		const float TargetSwaySize = TargetSway.Size();
		if (FMath::IsNearlyEqual(CurrentSway.X, TargetSway.X, 0.1f) && FMath::IsNearlyEqual(CurrentSway.Y, TargetSway.Y, 0.1f))
		{
			TargetSway = GenerateSway(SwayRadius * VelocitySwayRadiusMultiplier, 0.2f, 0.3f, 0.4f, TimeOffset);
		}
		else
		{
			// Then interpolate CurrentSway towards TargetSway
			CurrentSway = FMath::Vector2DInterpConstantTo(CurrentSway, TargetSway, DeltaTime, SwayInterpolationRate * VelocitySwayInterpolationMultiplier);	
		}
	}
	// Add Aim Stabilization time. When aiming after X time, sway will start stabiling.
	// Add aim Destabilization time. When aiming to long sway will start destabilizing.
	
	InGunState->BroadcastOnSwayChanged(CurrentSway.X, TargetSway.X, CurrentSway.Y, TargetSway.Y);

	if (bIsFiring)
	{
		const float Level = 1;


		/* ---- 6. Apply to player controller -------------------------------- */
		const float FixedUpdateRate = WeaponStats->RecoilUpdateRate;

		// Add the current frame's delta time to our accumulator
		TimeAccumulator += DeltaTime;

		// Process as many fixed time steps as we've accumulated
		//while (TimeAccumulator >= FixedUpdateRate)
		{
			/* ---- 1. Flip test -------------------------------------------------- */
			if (FMath::FRand() < WeaponStats->GetFlipChancePerShotDirect(Level))
			{
				TargetBiasDir *= -1;
				BiasValue     *= WeaponStats->GetFlipDampenFactorDirect(Level);   // keep part of current bias
			}
			
			/* ---- 2. March bias toward the target side ------------------------- */
			BiasValue = FMath::Clamp(
				BiasValue + BiasAccelPerShot * TargetBiasDir,
				-1.f, 1.f);
			
			/* ---- 3. Strength multiplier from curve ---------------------------- */
			float StrengthMul = ComputeRecoilStrength(
			RecoilHeat,
				WeaponStats->GetPeakStrengthDirect(Level),
				WeaponStats->GetRampUpShotsDirect(Level),
				WeaponStats->GetGrowthRateDirect(Level),
				WeaponStats->GetOscAmplitudeDirect(Level),
				WeaponStats->GetOscPeriodDirect(Level),
				WeaponStats->GetNoiseJitterDirect(Level),
				WeaponStats->GetOscillationDownwardChanceDirect(Level)
			);
			
			// jitter so patterns never look scripted
			StrengthMul *= FMath::FRandRange(1.f - WeaponStats->GetRandomStrengthJitterDirect(Level),
											 1.f + WeaponStats->GetRandomStrengthJitterDirect(Level));
			
			// global scalar + stance/movement modifiers
			StrengthMul *= WeaponStats->GetBaseMultiplierDirect(1);
			
			/* ---- 4. Per-axis random kick magnitude ---------------------------- */
			float VertKick = FMath::FRandRange(WeaponStats->GetRecoilVerticalMinDirect(Level), WeaponStats->GetRecoilVerticalMaxDirect(Level));
			float HorzKick = FMath::FRandRange(WeaponStats->GetRecoilHorizontalMinDirect(Level), WeaponStats->GetRecoilHorizontalMaxDirect(Level));
			
			// first-shot special rule
			if (ShotsFired == 0 && !FMath::IsNearlyZero(WeaponStats->GetFirstShotBonusDirect(Level)))
			{
				VertKick += WeaponStats->GetFirstShotBonusDirect(Level);
			}
			
			// scale both axes
			VertKick *= StrengthMul;
			HorzKick *= StrengthMul;
			
			///VertKick *= RecoilPitchMultiplier;
			///HorzKick *= RecoilYawMultiplier;
			
			/* ---- 5. Decide left/right sign using bias ------------------------- */
			float PRight = 0.5f + BiasValue * 0.5f;         // map -1..+1 → 0..1
			float Sign   = (FMath::FRand() < PRight) ? +1.f : -1.f;
			HorzKick    *= Sign;
			
			// Apply a fixed portion of the recoil per update
			// This gives us a consistent recoil rate regardless of frame rate
			ArcPC->ControlRotationOffset.Pitch += VertKick;
			ArcPC->ControlRotationOffset.Yaw += HorzKick;

			const FVector2D LocalSpaceRecoilNew = GenerateSemiRandomSpreadPattern(
				WeaponStats->GetLocalSpaceRecoilBaseDirect(Level)
				, WeaponStats->GetLocalSpaceRecoilHorizontalBiasDirect(Level) // Range: -1.0 (left) to 1.0 (right)
				, WeaponStats->GetLocalSpaceRecoilVerticalBiasDirect(Level) // Range: -1.0 (down) to 1.0 (up)
				, WeaponStats->GetLocalSpaceRecoilHeatScaleDirect(Level) // How strongly to follow the main bias (0-1)
				, WeaponStats->GetLocalSpaceRecoilOscillationPeriodDirect(Level) // Chance to temporarily move in opposite direction (0-1)
				, WeaponStats->GetLocalSpaceRecoilOscillationStrengthDirect(Level) // How strong the oscillation movement is (0-1)
				, 0//StatsFragment->GetLocalSpaceRecoilOscillationDownwardChanceDirect(Level) // How likely to oscillate downward (0-1)
				, WeaponStats->GetLocalSpaceRecoilHorizontalTightnessDirect(Level) // Controls horizontal spread tightness (0-1)
				, ShotsFired
				);
	
			UE_LOG(LogTemp, Log, TEXT("LocalSpaceRecoilNew %s"), *LocalSpaceRecoilNew.ToString());
			TargetScreenRecoil += LocalSpaceRecoilNew;
			
			
			// Subtract the fixed time step from our accumulator
			TimeAccumulator -= FixedUpdateRate;
			TimeAccumulator += 0.001;
		}

		TargetScreenRecoil.X = FMath::Clamp(TargetScreenRecoil.X, WeaponStats->GetLocalSpaceRecoilHorizontalMinValueDirect(Level), WeaponStats->GetLocalSpaceRecoilHorizontalMaxValueDirect(Level));
		TargetScreenRecoil.Y = FMath::Clamp(TargetScreenRecoil.Y, WeaponStats->GetLocalSpaceRecoilVerticalMinValueDirect(Level), WeaponStats->GetLocalSpaceRecoilVerticalMaxValueDirect(Level));
	}
	
	const float TimeSinceFired = InGunState->GetWorld()->TimeSince(LastFireTime);
	if (TimeSinceFired > SpreadHeatRecoveryDelay)
	{
		const float CooldownRate = WeaponStats->HeatToSpreadRecoveryPerSecondCurve->GetFloatValue(SpreadHeat)* SpreadRecoveryMultiplier;
		
		SpreadHeat = ClampSpreadHeat(WeaponStats, SpreadHeat - (CooldownRate * DeltaTime));

		CurrentSpreadAngle = (WeaponStats->HeatToSpreadCurve->GetFloatValue(SpreadHeat) + VelocitySpreadAdditive + RotationSpreadAdditive) * SpreadAngleMultiplier;
	}
	else
	{
		CurrentSpreadAngle = (WeaponStats->HeatToSpreadCurve->GetFloatValue(SpreadHeat) + VelocitySpreadAdditive + RotationSpreadAdditive) * SpreadAngleMultiplier;
	}
	
	InGunState->BroadcastOnSpreadChanged(CurrentSpreadAngle, CurrentSpreadAngle);

	const FVector2D FinaLocalSpaceRecoil = TargetScreenRecoil + CurrentSway;
	
	if (TimeSinceFired > RecoilHeatRecoveryDelay)
	{
		ScreenRecoil = FMath::Vector2DInterpConstantTo(ScreenRecoil, FinaLocalSpaceRecoil, DeltaTime, ScreenRecoilRecoveryInterpolationSpeed);
		
		const float CooldownRate = WeaponStats->HeatToRecoilRecoveryPerSecond->GetFloatValue(RecoilHeat) * RecoilRecoveryMultiplier;

		if (HighestAxisRotation > 0)
		{
			ArcPC->ClampValue.Reset();
			ArcPC->ControlRotationOffset.Pitch = 0;
			bBreakRecovery = true;
		}
		
		RecoilHeat = ClampRecoilHeat(WeaponStats, RecoilHeat - (CooldownRate * DeltaTime));
		if (!bIsFiring && !bBreakRecovery)
		{
			const ArcGunStats* StatsFragment = InGunState->GetSelectedGun().GunItemPtr->GetItemDefinition()->GetScalableFloatFragment<ArcGunStats>();
			ArcPC->ControlRotationOffset.Pitch = RecoveryDirection * StatsFragment->GetRecoilRecoverySpeedDirect(1);
			RecoveredInputValue = SmoothRecoveredInputValue(RecoveredInputValue, 6.1f, 0.35f);
			
			if (StartYInputValue < 0 && ArcPC->InputValue.Y <= StartYInputValue)
			{
				ArcPC->ClampValue.Reset();
				bBreakRecovery = true;
				ArcPC->ControlRotationOffset.Pitch = 0;
			}

			if (StartYInputValue > 0 && ArcPC->InputValue.Y <= StartYInputValue)
            {
				ArcPC->ClampValue.Reset();
            	bBreakRecovery = true;
            	ArcPC->ControlRotationOffset.Pitch = 0;
            }
		}
	}
	else
	{
		ScreenRecoil = FMath::Vector2DInterpTo(ScreenRecoil, FinaLocalSpaceRecoil, DeltaTime, ScreenRecoilInterpolationSpeed);
	}
}
