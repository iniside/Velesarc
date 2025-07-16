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



#include "ArcTargetingTask_GunEyeWithSpread.h"

#include "ArcGunRecoilInstance.h"
#include "ArcGunStateComponent.h"
#include "GameFramework/Character.h"
#include "Player/ArcCorePlayerState.h"
#include "Targeting/ArcTargetingSourceContext.h"

UArcTargetingTask_GunEyeWithSpread::UArcTargetingTask_GunEyeWithSpread(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcTargetingTask_GunEyeWithSpread::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& HitSet = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (Ctx == nullptr)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	AArcCorePlayerState* ArcPS = Cast<AArcCorePlayerState>(Ctx->InstigatorActor.Get());

	if (ArcPS == nullptr)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	UArcGunStateComponent* GunStateComponent = ArcPS->FindComponentByClass<UArcGunStateComponent>();
	if (GunStateComponent == nullptr)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	const FArcGunRecoilInstance* Recoil = GunStateComponent->GetGunRecoil<FArcGunRecoilInstance>();
	if (Recoil == nullptr)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Ctx->SourceActor.Get());
	if (Character == nullptr)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}
	
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Ctx->InstigatorActor);
	Params.AddIgnoredActor(Ctx->SourceActor);

	FVector Loc;
	FRotator Rot;
	Ctx->SourceActor->GetActorEyesViewPoint(Loc, Rot);


	
	const FVector RecoilDirection = Recoil->GetScreenORecoilffset(Rot.Vector());
	FVector EndLocation = Loc + (RecoilDirection * 10000.f);
			
	Ctx->SourceActor->GetWorld()->LineTraceSingleByChannel(HitResult, Loc, EndLocation, ECC_Visibility, Params);

	if (HitResult.bBlockingHit)
	{
		DrawDebugLine(ArcPS->GetWorld(), Loc, HitResult.ImpactPoint, FColor::Yellow, false);
		DrawDebugPoint(ArcPS->GetWorld(), HitResult.ImpactPoint, 15.f, FColor::Yellow, false);
	}
	else
	{
		DrawDebugLine(ArcPS->GetWorld(), Loc, HitResult.TraceEnd, FColor::Red, false);
		DrawDebugPoint(ArcPS->GetWorld(), HitResult.TraceEnd, 15.f, FColor::Red, false);
	}

	
	const FVector HeadLocation = Character->GetActorLocation() + (FVector::UpVector * 60);
	const FVector AdjustedStartLocation = HeadLocation + (Rot.Vector() * 40.f);
	const FVector EndDirection = (HitResult.bBlockingHit ? HitResult.ImpactPoint : HitResult.TraceEnd) - AdjustedStartLocation;
	const FVector EndLocationAdjusted = AdjustedStartLocation + (EndDirection.GetSafeNormal() * 10000.f);

	FHitResult RealHit;
	Ctx->SourceActor->GetWorld()->LineTraceSingleByChannel(RealHit, AdjustedStartLocation, EndLocationAdjusted, ECC_Visibility, Params);

	int32 Idx = HitSet.TargetResults.AddDefaulted();
	HitSet.TargetResults[Idx].HitResult = RealHit;

	if (HitResult.bBlockingHit)
	{
		DrawDebugLine(ArcPS->GetWorld(), AdjustedStartLocation, RealHit.ImpactPoint, FColor::Yellow, false);
		DrawDebugPoint(ArcPS->GetWorld(), RealHit.ImpactPoint, 15.f, FColor::Yellow, false);
	}
	else
	{
		DrawDebugLine(ArcPS->GetWorld(), AdjustedStartLocation, RealHit.TraceEnd, FColor::Red, false);
		DrawDebugPoint(ArcPS->GetWorld(), RealHit.TraceEnd, 15.f, FColor::Red, false);
	}
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}
