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

#include "ArcGunTypes.h"

#include "ArcGunActor.h"
#include "ArcGunRecoilInstance.h"
#include "ArcGunStateComponent.h"
#include "ArcCore/Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemData.h"
#include "Player/ArcCorePlayerState.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Targeting/ArcTargetingSourceContext.h"

FArcSelectedGun::FArcSelectedGun(const FArcItemData* InWeaponItemPtr): GunItemPtr(InWeaponItemPtr)
{
	if(InWeaponItemPtr == nullptr)
	{
		return;
	}
}

void FArcSelectedGun::SetWeaponItemPtr(const FArcItemData* InWeaponItemPtr)
{
	if(InWeaponItemPtr == nullptr)
	{
		return;
	}
	GunItemPtr = InWeaponItemPtr;
}

UArcTargetingTask_GunTrace::UArcTargetingTask_GunTrace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcTargetingTask_GunTrace::Init(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Init(TargetingHandle);
}

void UArcTargetingTask_GunTrace::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Execute(TargetingHandle);

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);
	
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet* HitSet = FTargetingDefaultResultsSet::Find(TargetingHandle);

	if (Ctx == nullptr || HitSet == nullptr)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	if (HitSet->TargetResults.Num() == 0)
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

	AArcGunActor* GunActor = GunStateComponent->GetSelectedGun().GunActor.Get();
	
	USkeletalMeshComponent* SMC = GunActor ? GunActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;

	USkeletalMeshComponent* SMCCharacter = Ctx->SourceActor->FindComponentByClass<USkeletalMeshComponent>();
	
	FHitResult& CameraHit = HitSet->TargetResults[0].HitResult;

	FVector HitLoc = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : CameraHit.TraceEnd;
	
	FVector StartLocation;

	ACharacter* C = Cast<ACharacter>(Ctx->SourceActor);
	
	if (SMC)
	{
		StartLocation = SMCCharacter->GetSocketLocation(SocketName);
	}
	else
	{
		FRotator Rot;
		Ctx->SourceActor->GetActorEyesViewPoint(StartLocation, Rot);	
	}
	FTransform SocketTransform = SMCCharacter->GetSocketTransform(SocketName);

	

	FVector Loc;
	FRotator Rot;
	Ctx->SourceActor->GetActorEyesViewPoint(Loc, Rot);
	
	//LocaLoc = SocketTransform.GetRotation().Rotator().Vector();
	//const FVector Direction = Rot.Vector();
	FVector Direction = (HitLoc - StartLocation).GetSafeNormal();
	
	//Direction = (Direction + LocaLoc.GetSafeNormal()).GetSafeNormal();
	
	//const FVector Direction = SocketTransform.GetRotation().Rotator().Vector();
	
	const FArcGunRecoilInstance* Recoil = GunStateComponent->GetGunRecoil<FArcGunRecoilInstance>();
	if (Recoil == nullptr)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}
	const FVector RecoilDirection = Recoil->GetScreenORecoilffset(Direction);
	
	FVector EndLocation = StartLocation + (RecoilDirection * 10000.f);

	//DrawDebugLine(Ctx->InstigatorActor->GetWorld()
	//	, CameraHit.TraceStart, HitLoc
	//	, FColor::Blue, false, 0, 0, 0.2f);

	//DrawDebugPoint(Ctx->InstigatorActor->GetWorld()
	//, HitLoc
	//, 5.f, FColor::Blue);
	
	//DrawDebugLine(Ctx->InstigatorActor->GetWorld()
	//	, StartLocation, EndLocation
	//	, FColor::Yellow, false, 0, 0, 0.2f);
	
	//DrawDebugPoint(Ctx->InstigatorActor->GetWorld()
	//, EndLocation
	//, 5.f, FColor::Yellow);
	
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GunActor);
	Params.AddIgnoredActor(Ctx->InstigatorActor.Get());
	Params.AddIgnoredActor(Ctx->SourceActor.Get());
	
	Ctx->InstigatorActor->GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, Params);
	HitSet->TargetResults.Empty();
		
	int32 Idx = HitSet->TargetResults.AddDefaulted();
	HitSet->TargetResults[Idx].HitResult = HitResult;

	const FVector DebugEnd = HitResult.bBlockingHit ? HitResult.ImpactPoint : HitResult.TraceEnd;
	
	//DrawDebugLine(Ctx->InstigatorActor->GetWorld()
	//	, HitResult.TraceStart, DebugEnd
	//	, FColor::Red, false, 0, 0, 0.2f);

	//DrawDebugPoint(Ctx->InstigatorActor->GetWorld()
	//	, DebugEnd
	//	, 5.f, FColor::Red);
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

void UArcTargetingTask_GunTrace::CancelAsync() const
{
	Super::CancelAsync();
}

#if ENABLE_DRAW_DEBUG
void UArcTargetingTask_GunTrace::DrawDebug(UTargetingSubsystem* TargetingSubsystem
	, FTargetingDebugInfo& Info
	, const FTargetingRequestHandle& TargetingHandle
	, float XOffset
	, float YOffset
	, int32 MinTextRowsToAdvance) const
{
	
}
#endif