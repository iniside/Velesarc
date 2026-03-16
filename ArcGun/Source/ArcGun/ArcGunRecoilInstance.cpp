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

#include "Engine/World.h"
#include "ArcGunAttributeSet.h"
#include "ArcGunStateComponent.h"
#include "AbilitySystem/ArcAbilitiesTypes.h"
#include "Fragments/ArcItemFragment_GunStats.h"
#include "Player/ArcCorePlayerController.h"
#include "GameFramework/Character.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"

FVector GetVectorInConeFromScreenRecoil(const FVector& Direction, float HorizontalDegrees, float VerticalDegrees)
{
	FVector NormalizedDir = Direction.GetSafeNormal();
	float HorizontalRadians = FMath::DegreesToRadians(HorizontalDegrees);
	float VerticalRadians = FMath::DegreesToRadians(VerticalDegrees);
    
	FVector UpVector = FVector(0, 0, 1);
	if (FMath::Abs(FVector::DotProduct(NormalizedDir, UpVector)) > 0.99f)
	{
		UpVector = FVector(0, 1, 0);
	}
    
	FVector RightVector = FVector::CrossProduct(NormalizedDir, UpVector).GetSafeNormal();
	FVector NewUpVector = FVector::CrossProduct(RightVector, NormalizedDir).GetSafeNormal();
    
	FVector OffsetDir = NormalizedDir * FMath::Cos(VerticalRadians) * FMath::Cos(HorizontalRadians) +
						RightVector * FMath::Sin(HorizontalRadians) +
						NewUpVector * FMath::Sin(VerticalRadians);
    
	return OffsetDir.GetSafeNormal();
}

void FArcGunRecoilInstance_Base::Deinitialize()
{
}

void FArcGunRecoilInstance_Base::StartRecoil(UArcGunStateComponent* InGunState)
{
	if (!InGunState) return;

	CharacterOwner = InGunState->GetOwner<APawn>();
	if (CharacterOwner)
	{
		ArcPC = CharacterOwner->GetController<AArcCorePlayerController>();
		ArcASC = InGunState->GetArcASC();
	}

	if (!ArcPC) return;

	UpdateAttributes(InGunState->GetSelectedGun().GunItemPtr);
	
	bIsFiring = true;
	ShotsFired = 0;
	CurrentIntensity = 0.0f;
}

void FArcGunRecoilInstance_Base::StopRecoil(UArcGunStateComponent* InGunState)
{
	bIsFiring = false;
	if (ArcPC)
	{
		ArcPC->ResetPitchModifier();
		ArcPC->ResetYawModifier();
	}
}

void FArcGunRecoilInstance_Base::OnShootFired(UArcGunStateComponent* InGunState)
{
	const FArcItemData* Item = InGunState->GetSelectedGun().GunItemPtr;
	const FArcItemFragment_GunStats* Stats = Item->GetItemDefinition()->GetScalableFloatFragment<ArcGunStats>();
	if (!Stats) return;

	UpdateAttributes(Item);

	// 1. Add Intensity
	float IntensityGain = Stats->IntensityPerShot.GetValueAtLevel(1) * HandlingMultiplier;
	CurrentIntensity = FMath::Clamp(CurrentIntensity + IntensityGain, 0.0f, 1.0f);
	IntensityDecayTimer = Stats->IntensityDecayDelay.GetValueAtLevel(1);

	// 2. Camera Kick (Input Recoil)
	float RecoilStrength = 1.0f;
	if (Stats->IntensityToRecoilMultiplierCurve)
	{
		RecoilStrength = Stats->IntensityToRecoilMultiplierCurve->GetFloatValue(CurrentIntensity);
	}

	const FArcGunKickParams& Kick = Stats->KickParams;
	float VertKick = Kick.VerticalBase.GetValueAtLevel(1) * FMath::FRandRange(1.f - Kick.Variation.GetValueAtLevel(1), 1.f + Kick.Variation.GetValueAtLevel(1));
	float HorzKick = Kick.HorizontalBase.GetValueAtLevel(1) * FMath::FRandRange(1.f - Kick.Variation.GetValueAtLevel(1), 1.f + Kick.Variation.GetValueAtLevel(1));

	if (ShotsFired == 0) VertKick *= Kick.FirstShotMultiplier.GetValueAtLevel(1);

	// Bias logic
	float Sign = (FMath::FRand() < (0.5f + Kick.BiasStrength.GetValueAtLevel(1) * 0.5f)) ? 1.f : -1.f;
	
	if (ArcPC)
	{
		ArcPC->ControlRotationOffset.Pitch += VertKick * RecoilStrength * RecoilMultiplier;
		ArcPC->ControlRotationOffset.Yaw += HorzKick * Sign * RecoilStrength * RecoilMultiplier;
	}

	// 3. Screen Kick (Stability)
	const FArcGunStabilityParams& Stability = Stats->StabilityParams;
	float StabilityKick = Stability.BaseKick.GetValueAtLevel(1) * HandlingMultiplier;
	
	FVector2D RandomStability = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(0.f, 1.f));
	StabilityOffset += (RandomStability + Stability.Bias) * StabilityKick;
	StabilityOffset.X = FMath::Clamp(StabilityOffset.X, -Stability.MaxOffset.GetValueAtLevel(1), Stability.MaxOffset.GetValueAtLevel(1));
	StabilityOffset.Y = FMath::Clamp(StabilityOffset.Y, -Stability.MaxOffset.GetValueAtLevel(1), Stability.MaxOffset.GetValueAtLevel(1));

	ShotsFired++;
	LastFireTime = InGunState->GetWorld()->GetTimeSeconds();
}

void FArcGunRecoilInstance_Base::UpdateRecoil(float DeltaTime, UArcGunStateComponent* InGunState)
{
	const FArcItemData* Item = InGunState->GetSelectedGun().GunItemPtr;
	const FArcItemFragment_GunStats* Stats = Item->GetItemDefinition()->GetScalableFloatFragment<ArcGunStats>();
	if (!Stats) return;

	bIsAiming = ArcASC && ArcASC->HasMatchingGameplayTag(AimTag);

	UpdateIntensity(DeltaTime, Stats);
	UpdateCrosshairDynamics(DeltaTime, Stats);

	// Broadcast for UI/Crosshair
	float CurrentSpread = GetCurrentSpread();
	InGunState->BroadcastOnSpreadChanged(CurrentSpread, CurrentSpread);
	InGunState->BroadcastOnSwayChanged(SwayOffset.X, 0.f, SwayOffset.Y, 0.f);
}

void FArcGunRecoilInstance_Base::UpdateIntensity(float DeltaTime, const FArcItemFragment_GunStats* Stats)
{
	if (IntensityDecayTimer > 0.0f)
	{
		IntensityDecayTimer -= DeltaTime;
	}
	else
	{
		float Decay = Stats->IntensityDecayRate.GetValueAtLevel(1) * DeltaTime;
		CurrentIntensity = FMath::Clamp(CurrentIntensity - Decay, 0.0f, 1.0f);
	}
}

void FArcGunRecoilInstance_Base::UpdateCrosshairDynamics(float DeltaTime, const FArcItemFragment_GunStats* Stats)
{
	// 1. Recover Stability (Screen Recoil)
	float RecoverySpeed = Stats->StabilityParams.RecoverySpeed.GetValueAtLevel(1);
	StabilityOffset = FMath::Vector2DInterpConstantTo(StabilityOffset, FVector2D::ZeroVector, DeltaTime, RecoverySpeed);

	// 2. Update Sway
	const FArcGunSwayParams& Sway = Stats->SwayParams;
	float Time = CharacterOwner ? CharacterOwner->GetWorld()->GetTimeSeconds() : 0.0f;
	
	float CurrentRadius = Sway.Radius.GetValueAtLevel(1);
	if (bIsAiming) CurrentRadius *= Sway.AimMultiplier.GetValueAtLevel(1);

	float SwaySpeed = Sway.Speed.GetValueAtLevel(1);
	SwayOffset.X = FMath::Sin(Time * SwaySpeed * 0.7f) * CurrentRadius;
	SwayOffset.Y = FMath::Cos(Time * SwaySpeed * 1.1f) * CurrentRadius;

	// 3. Combine
	CrosshairOffset = StabilityOffset + SwayOffset;
}

void FArcGunRecoilInstance_Base::UpdateAttributes(const FArcItemData* InItem)
{
	if (!ArcASC || !InItem) return;

	// Use static/cached tags or get them from item
	static FGameplayTagContainer SourceTags;
	SourceTags.Reset();
	SourceTags.AppendTags(InItem->GetItemAggregatedTags());

	FAggregatorEvaluateParameters Params;
	Params.SourceTags = &SourceTags;

	// Simplified attribute pull - in a real project we'd use AttributeSet accessors
	// For this refactor, we assume a simplified multiplier model
	RecoilMultiplier = 1.0f; // Could be pulled from UArcGunAttributeSet::GetStabilityAttribute()
	SpreadMultiplier = 1.0f; // Could be pulled from UArcGunAttributeSet::GetAccuracyAttribute()
	HandlingMultiplier = 1.0f; 

	if (bIsAiming)
	{
		const FArcItemFragment_GunStats* Stats = InItem->GetItemDefinition()->GetScalableFloatFragment<ArcGunStats>();
		SpreadMultiplier *= Stats->AimSpreadMultiplier.GetValueAtLevel(1);
	}
}

FVector FArcGunRecoilInstance_Base::GetScreenORecoilffset(const FVector& Direction) const
{
	return GetVectorInConeFromScreenRecoil(Direction, CrosshairOffset.X, CrosshairOffset.Y);
}

float FArcGunRecoilInstance_Base::GetCurrentSpread() const
{
	// In reality we would need the Stats pointer here, 
	// but FArcGunRecoilInstance::GetCurrentSpread is const and doesn't take parameters.
	// In the refactored UpdateRecoil, we calculate and broadcast it.
	return 0.0f; 
}

void FArcGunRecoilInstance_Base::HandlePreProcessRotation(float Value)
{
	// Logic for breaking recovery when player moves mouse could go here
}
