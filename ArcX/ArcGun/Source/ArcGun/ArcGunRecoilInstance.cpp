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
	
		const float RecoilAimMultiplier = 1.f;// bIsAiming ? InItem->GetValue(ArcGunStats::GetRecoilAimMultiplierData()) : 1;

		RecoilHeatRecoveryDelay = 0.2;//InItem->GetValue(ArcGunStats::GetRecoilHeatRecoveryDelayData());
		RecoilYawMultiplier = RecoilAimMultiplier / RecoilAttributeFinalValue;
		RecoilPitchMultiplier = RecoilAimMultiplier / RecoilAttributeFinalValue;
	
		RecoilHeatGainMultiplier = 1 / RecoilAttributeFinalValue;
	
		ScreenRecoilInterpolationSpeed = 1; //ArcGunStats::GetScreenRecoilGainInterpolationSpeedValue(InItem);
		ScreenRecoilRecoveryInterpolationSpeed = 1;//ArcGunStats::GetScreenRecoilRecoveryInterpolationSpeedValue(InItem);
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
	
	FRotator Unused;
	CharacterOwner->GetActorEyesViewPoint(StartCameraLocation, Unused);
	
	RecoveredPitch = 0;
	bBreakRecovery = true;
;
	bIsFiring = true;
	
	const ArcGunStats* StatsFragment = InGunState->GetSelectedGun().GunItemPtr->GetItemDefinition()->GetScalableFloatFragment<ArcGunStats>();
	
	ShotsFired        = 0;
	BiasValue         = 0.f;
	
	AccumulatedPhase = 0;
	LocalSpaceRecoilAccumulatedPhase = 0;

	float BiasStrength = StatsFragment->GetBiasStrengthDirect(1); 
	TargetBiasDir = FMath::Sign(BiasStrength);
	if (FMath::IsNearlyZero(BiasStrength))
	{
		TargetBiasDir = (FMath::FRand() < 0.5f) ? -1.f : 1.f; // Random if neutral
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
	NewStability = FVector2D::ZeroVector;
	
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

FVector2D FArcGunRecoilInstance_Base::GenerateStabilitySpreadPattern(
	float BaseKick,
    float MaxSpread,           // Base spread amount (0.5-2.0 typical)
    float HorizontalBias,       // -1.0 (left) to 1.0 (right)
    float VerticalBias,         // -1.0 (down) to 1.0 (up)
    float PatternTightness,     // 0.0 (chaotic) to 1.0 (tight pattern)
    float OscillationRate,      // 0.0 to 2.0 - how often pattern changes
    FVector2D& CurrentOffset
)
{
	/* ---- 1. Generate This Shot's Kick Direction ---- */
	float RandomH = FMath::FRandRange(-1.0f, 1.0f);
	float RandomV = FMath::FRandRange(0.0f, 1.0f);
    
	/* ---- 2. Apply Oscillation as VARIATION, not replacement ---- */
	float OscillationH = 0.0f;
	float OscillationV = 0.0f;

	if (OscillationRate > 0.0f)
	{
		float OscPhase = ShotsFired * OscillationRate * 0.4f;
        
		// Oscillation adds EXTRA movement, doesn't replace the base pattern
		OscillationH = FMath::Sin(OscPhase) * 0.3f;         // ±30% extra horizontal movement
		OscillationV = FMath::Sin(OscPhase * 0.7f) * 0.2f;  // ±20% extra vertical movement
	}
    
	/* ---- 3. Build Pattern Direction Based on Tightness ---- */
	float BiasInfluence = PatternTightness * 0.8f;
	float RandomInfluence = 1.0f - BiasInfluence;
    
	// Blend random movement with bias direction
	float BaseH = (RandomH * RandomInfluence) + (HorizontalBias * BiasInfluence);
	float BaseV = (RandomV * RandomInfluence) + (VerticalBias * BiasInfluence);
    
	// Add oscillation as variation ON TOP of the base pattern
	float ThisShotH = BaseH + OscillationH;
	float ThisShotV = BaseV + OscillationV;
    
	/* ---- 4. Scale by Base Kick Strength ---- */
	FVector2D ThisShotKick = FVector2D(ThisShotH, ThisShotV) * BaseKick;

	float HeatMultiplier = 1.0f + (ShotsFired * 0.1f); // 10% increase per shot
	HeatMultiplier = FMath::Clamp(HeatMultiplier, 1.0f, 3.0f); 
	ThisShotKick *= HeatMultiplier;

	/* ---- 5. Add to Cumulative Offset ---- */
	FVector2D NewOffset = CurrentOffset + ThisShotKick;

    
	NewOffset.X = FMath::Clamp(NewOffset.X, -MaxSpread, MaxSpread);
	NewOffset.Y = FMath::Clamp(NewOffset.Y, -MaxSpread, MaxSpread);
	return NewOffset; // Return the total accumulated offset

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
	ShotsFired++;
	
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

	const float TickRate = 1 / WeaponStats->FramerateUpdateRate;
	if (bIsFiring)
	{
		const float Level = 1;

		// Add the current frame's delta time to our accumulator
		TimeAccumulator += DeltaTime;

		// Process as many fixed time steps as we've accumulated
		while (TimeAccumulator > TickRate)
		{

    		/* ---- 1. Simplified Bias System ---- */
    		// Combine flip chance and bias acceleration into one parameter
    		float BiasStrength = WeaponStats->GetBiasStrengthDirect(Level); // New combined parameter (0.0-1.0)
			float AbsBiasStrength = FMath::Abs(BiasStrength);

			if (ShotsFired == 0) // Initialize on first shot
			{

			}
    
			// Flip chance based on bias strength magnitude
			if (FMath::FRand() < AbsBiasStrength * 0.2f) // 20% of bias strength becomes flip chance
			{
				TargetBiasDir *= -1;
				BiasValue *= 0.7f; // Fixed damping factor
			}
    
			// March bias toward target direction
			float BiasAcceleration = AbsBiasStrength * 0.1f;
			BiasValue = FMath::Clamp(
				BiasValue + BiasAcceleration * TargetBiasDir,
				-1.f, 1.f);
    
			// Apply the weapon's inherent directional bias
			float WeaponBias = FMath::Clamp(BiasStrength * 0.3f, -0.3f, 0.3f); // Cap weapon bias influence
			float FinalBiasValue = FMath::Clamp(BiasValue + WeaponBias, -1.f, 1.f);
   
    		
			/* ---- 2. Simplified Strength Calculation ---- */
			float HeatRatio = FMath::Clamp(RecoilHeat / WeaponStats->GetMaxHeatDirect(Level), 0.f, 1.f);
			float StrengthMul = FMath::Lerp(1.0f, WeaponStats->GetMaxStrengthDirect(Level), HeatRatio);
    
			// Simple oscillation (optional)
			float OscillationFrequency = WeaponStats->GetOscillationFrequencyDirect(Level);
			if (OscillationFrequency > 0.0f)
			{
				// Use shot count as time base for consistent oscillation regardless of fire rate
				float OscillationPhase = ShotsFired * OscillationFrequency;
				float OscillationValue = FMath::Sin(OscillationPhase) * 0.2f + 1.0f; // ±20% variation around 1.0
				StrengthMul *= OscillationValue;
			}

    
			// Simplified jitter
			float JitterAmount = WeaponStats->GetJitterAmountDirect(Level);
			StrengthMul *= FMath::FRandRange(1.f - JitterAmount, 1.f + JitterAmount);
    
			/* ---- 3. Simplified Kick Magnitude ---- */
			float BaseVertical = WeaponStats->GetVerticalRecoilBaseDirect(Level);
			float BaseHorizontal = WeaponStats->GetHorizontalRecoilBaseDirect(Level);
			float RecoilVariation = WeaponStats->GetRecoilVariationDirect(Level);
    
			float VertKick = BaseVertical * FMath::FRandRange(1.f - RecoilVariation, 1.f + RecoilVariation);
			float HorzKick = BaseHorizontal * FMath::FRandRange(1.f - RecoilVariation, 1.f + RecoilVariation);
    
			// First shot bonus
			if (ShotsFired == 0)
			{
				VertKick *= WeaponStats->GetFirstShotMultiplierDirect(Level);
			}
    
			// Apply strength multiplier
			VertKick *= StrengthMul;
			HorzKick *= StrengthMul;
    
			/* ---- 4. Apply Final Bias for Left/Right Direction ---- */
			// Convert bias value (-1 to 1) to probability (0 to 1)
			float PRight = 0.5f + FinalBiasValue * 0.5f; // -1 becomes 0 (always left), +1 becomes 1 (always right)
			float Sign = (FMath::FRand() < PRight) ? +1.f : -1.f;
			HorzKick *= Sign;


			// Apply a fixed portion of the recoil per update
			// This gives us a consistent recoil rate regardless of frame rate
			ArcPC->ControlRotationOffset.Pitch += VertKick;
			ArcPC->ControlRotationOffset.Yaw += HorzKick;

			NewStability = GenerateStabilitySpreadPattern(
				WeaponStats->GetStabilityBaseKickDirect(Level)
				, WeaponStats->GetMaxStabilitySpreadDirect(Level)
				, WeaponStats->GetStabilityHorizontalBiasDirect(Level)
				, WeaponStats->GetStabilityVerticalBiasDirect(Level)
				, WeaponStats->GetStabilityPatternTightnessDirect(Level)
				, WeaponStats->GetStabilityOscillationRateDirect(Level)
				, TargetScreenRecoil
				);

			TimeAccumulator -= TickRate;
			TimeAccumulator += 0.001;
		}
		//TargetScreenRecoil = NewStability;
		//
	}

	TargetScreenRecoil = FMath::Vector2DInterpConstantTo(TargetScreenRecoil, NewStability, DeltaTime, WeaponStats->GetStabilityGainSpeedDirect(1));
	
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
			ArcPC->ControlRotationOffset.Pitch = RecoveryDirection * 1.f;//StatsFragment->GetRecoilRecoverySpeedDirect(1);
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
		TargetScreenRecoil = FMath::Vector2DInterpConstantTo(TargetScreenRecoil, FVector2D::ZeroVector, DeltaTime, WeaponStats->GetStabilityGainSpeedDirect(1));
	}
}
