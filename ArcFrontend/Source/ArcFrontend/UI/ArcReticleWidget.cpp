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

#include "UI/ArcReticleWidget.h"

#include "ArcGunStateComponent.h"
#include "ArcWorldDelegates.h"
#include "GameFramework/PlayerState.h"
#include "Player/ArcCorePlayerState.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingSubsystem.h"

void UArcReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	
	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwningPlayerState());

	if (GunComponent == nullptr)
	{
		UArcWorldDelegates::Get(GetOwningPlayerState())->AddActorOnComponentBeginPlay(
			FArcComponentChannelKey(GetOwningPlayerState(), UArcGunStateComponent::StaticClass())
				, FArcGenericActorComponentDelegate::FDelegate::CreateUObject(
					this, &UArcReticleWidget::HandleGunStateComponentBeginPlay
				)
			);
		return;
	}
	
	WeaponSpreadDelegate = GunComponent->AddOnSpreadChanged(FRecoilSpreadChanged::FDelegate::CreateUObject(
		this, &UArcReticleWidget::ComputeSpreadAngle));

	WeaponSwaydDelegate = GunComponent->AddOnSwayChanged(FRecoilSwayChanged::FDelegate::CreateUObject(
		this, &UArcReticleWidget::ComputeSwayAngle));
	
	GunComponent->AddOnPreShoot(FArcOnShootFired::FDelegate::CreateUObject(
		this, &UArcReticleWidget::HandleOnWeaponFireHit));
}

void UArcReticleWidget::NativeDestruct()
{
	Super::NativeDestruct();
	if (GetOwningPlayerState())
	{
		UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwningPlayerState());
		if(GunComponent != nullptr)
		{
			GunComponent->RemoveOnSpreadChanged(WeaponSpreadDelegate);
		}
	}
}

void UArcReticleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (TargetingPreset)
	{
		FArcTargetingSourceContext Context;
		Context.SourceActor = GetOwningPlayer()->GetPawn();
		Context.InstigatorActor = GetOwningPlayer()->GetPlayerState<AArcCorePlayerState>();
		Context.SourceObject = this;
		
		UTargetingSubsystem* TargetingSubsystem = UTargetingSubsystem::Get(GetWorld());

		FTargetingRequestHandle RequestHandle = Arcx::MakeTargetRequestHandle(TargetingPreset, Context);
		
		TargetingSubsystem->ExecuteTargetingRequestWithHandle(RequestHandle
			, FTargetingRequestDelegate::CreateUObject(this, &UArcReticleWidget::HandleOnTargetingReady));
	}
}

void UArcReticleWidget::HandleOnTargetingReady(FTargetingRequestHandle Handle)
{
	FTargetingDefaultResultsSet* TargetingResults = FTargetingDefaultResultsSet::Find(Handle);
	if (TargetingResults)
	{
		if (TargetingResults->TargetResults.Num() > 0)
		{
			OnTargetHitReady(TargetingResults->TargetResults[0].HitResult);		
		}
	}
	
}

void UArcReticleWidget::ComputeSpreadAngle(float CurrentSpread, float MaxSpread)
{
	//if (const ULyraRangedWeaponInstance* RangedWeapon = Cast<const ULyraRangedWeaponInstance>(WeaponInstance))

	float CalculatedSpread = 0;

	{
		//const float BaseSpreadAngle = RangedWeapon->GetCalculatedSpreadAngle();
		//const float SpreadAngleMultiplier = RangedWeapon->GetCalculatedSpreadAngleMultiplier();
		//const float ActualSpreadAngle = BaseSpreadAngle * SpreadAngleMultiplier;

	
		const float LongShotDistance = 10000.f;

		APlayerController* PC = GetOwningPlayer();
		if (PC && PC->PlayerCameraManager)
		{
			// A weapon's spread can be thought of as a cone shape. To find the screenspace spread for reticle visualization,
			// we create a line on the edge of the cone at a long distance. The end of that point is on the edge of the cone's circle.
			// We then project it back onto the screen. Its distance from screen center is the spread radius.

			// This isn't perfect, due to there being some distance between the camera location and the gun muzzle.

			const float SpreadRadiusRads = FMath::DegreesToRadians(CurrentSpread * 0.5f);
			const float SpreadRadiusAtDistance = FMath::Tan(SpreadRadiusRads) * LongShotDistance;

			FVector CamPos;
			FRotator CamOrient;
			PC->PlayerCameraManager->GetCameraViewPoint(CamPos, CamOrient);

			FVector CamForwDir = CamOrient.RotateVector(FVector::ForwardVector);
			FVector CamUpDir = CamOrient.RotateVector(FVector::UpVector);

			FVector OffsetTargetAtDistance = CamPos + (CamForwDir * (double)LongShotDistance) + (CamUpDir * (double)SpreadRadiusAtDistance);

			FVector2D OffsetTargetInScreenspace;

			if (PC->ProjectWorldLocationToScreen(OffsetTargetAtDistance, OffsetTargetInScreenspace, true))
			{
				int32 ViewportSizeX(0), ViewportSizeY(0);
				PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

				const FVector2D ScreenSpaceCenter(FVector::FReal(ViewportSizeX) * 0.5f, FVector::FReal(ViewportSizeY) * 0.5f);

				CalculatedSpread = BaseRadius + (OffsetTargetInScreenspace - ScreenSpaceCenter).Length();
			}
		}
	}

	OnSpreadChanged(CalculatedSpread);
}

void UArcReticleWidget::ComputeSwayAngle(float HorizontalSway
	, float MaxHorizontalSway
	, float VerticalSway
	, float MaxVerticalSway)
{
	float H = FMath::Abs( HorizontalSway );
	float V = FMath::Abs( VerticalSway );
	if (H > V)
	{
		ComputeSwayAngleImpl(H, MaxHorizontalSway);
	}
	else
	{
		ComputeSwayAngleImpl(V, MaxVerticalSway);
	}
}

void UArcReticleWidget::ComputeSwayAngleImpl(float Sway
	, float MaxSway)
{
	float CalculatedSpread = 0;

	{
		//const float BaseSpreadAngle = RangedWeapon->GetCalculatedSpreadAngle();
		//const float SpreadAngleMultiplier = RangedWeapon->GetCalculatedSpreadAngleMultiplier();
		//const float ActualSpreadAngle = BaseSpreadAngle * SpreadAngleMultiplier;

	
		const float LongShotDistance = 10000.f;

		APlayerController* PC = GetOwningPlayer();
		if (PC && PC->PlayerCameraManager)
		{
			// A weapon's spread can be thought of as a cone shape. To find the screenspace spread for reticle visualization,
			// we create a line on the edge of the cone at a long distance. The end of that point is on the edge of the cone's circle.
			// We then project it back onto the screen. Its distance from screen center is the spread radius.

			// This isn't perfect, due to there being some distance between the camera location and the gun muzzle.

			const float SpreadRadiusRads = FMath::DegreesToRadians(Sway * 0.5f);
			const float SpreadRadiusAtDistance = FMath::Tan(SpreadRadiusRads) * LongShotDistance;

			FVector CamPos;
			FRotator CamOrient;
			PC->PlayerCameraManager->GetCameraViewPoint(CamPos, CamOrient);

			FVector CamForwDir = CamOrient.RotateVector(FVector::ForwardVector);
			FVector CamUpDir = CamOrient.RotateVector(FVector::UpVector);

			FVector OffsetTargetAtDistance = CamPos + (CamForwDir * (double)LongShotDistance) + (CamUpDir * (double)SpreadRadiusAtDistance);

			FVector2D OffsetTargetInScreenspace;

			if (PC->ProjectWorldLocationToScreen(OffsetTargetAtDistance, OffsetTargetInScreenspace, true))
			{
				int32 ViewportSizeX(0), ViewportSizeY(0);
				PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

				const FVector2D ScreenSpaceCenter(FVector::FReal(ViewportSizeX) * 0.5f, FVector::FReal(ViewportSizeY) * 0.5f);

				CalculatedSpread = BaseRadius + (OffsetTargetInScreenspace - ScreenSpaceCenter).Length();
			}
		}
	}

	OnSwayChanged(CalculatedSpread);
}

//void UArcReticleWidget::HandleOnWeaponFireHit(
//			UGameplayAbility* InAbility
//			, const FArcSelectedGun& SelectedGun
//			, const FGameplayAbilityTargetDataHandle& InHits
//			, UArcTargetingObject* InTrace
//			, const FGameplayAbilitySpecHandle& RequestHandle)
//{
//	if(InHits.Num() == 0)
//	{
//		return;
//	}
//	
//	if(const FHitResult* HitResult = InHits.Get(0)->GetHitResult())
//	{
//		FVector HitLocation = HitResult->bBlockingHit ? HitResult->ImpactPoint : HitResult->TraceEnd;
//		OnWeaponFireHit(HitLocation);
//	}
//}

void UArcReticleWidget::HandleOnWeaponFireHit(
			const FArcGunShotInfo& ShotInfo)
{
	if(ShotInfo.Targets.Num() == 0)
	{
		return;
	}
	
	if(const FHitResult* HitResult = &ShotInfo.Targets[0])
	{
		FVector HitLocation = HitResult->bBlockingHit ? HitResult->ImpactPoint : HitResult->TraceEnd;
		OnWeaponFireHit(HitLocation);
	}
}

void UArcReticleWidget::HandleGunStateComponentBeginPlay(UActorComponent* InComponent)
{
	UArcWorldDelegates::Get(GetOwningPlayerState())->RemoveActorOnComponentBeginPlay(
	FArcComponentChannelKey(GetOwningPlayerState(), UArcGunStateComponent::StaticClass())
		, GunStateComponentBeginPlayDelegate);

	if (UArcGunStateComponent* GunComponent = Cast<UArcGunStateComponent>(InComponent))
	{
		WeaponSpreadDelegate = GunComponent->AddOnSpreadChanged(FRecoilSpreadChanged::FDelegate::CreateUObject(
		this, &UArcReticleWidget::ComputeSpreadAngle));

		GunComponent->AddOnPreShoot(FArcOnShootFired::FDelegate::CreateUObject(
			this, &UArcReticleWidget::HandleOnWeaponFireHit));
	}
}

void UArcImage::ArcSetBrushFromSoftTexture(TSoftObjectPtr<UTexture2D> SoftTexture)
{
	SetBrushFromSoftTexture(SoftTexture);
}

void UArcImage::SetBrushFromSoftObject(TSoftObjectPtr<UObject> SoftObject)
{
	SetBrushFromLazyDisplayAsset(SoftObject);
}
