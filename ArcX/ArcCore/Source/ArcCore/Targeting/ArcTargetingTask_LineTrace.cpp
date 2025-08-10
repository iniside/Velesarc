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

#include "Targeting/ArcTargetingTask_LineTrace.h"

#include "ArcTargetingSourceContext.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UArcTargetingTask_LineTrace::UArcTargetingTask_LineTrace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcTargetingTask_LineTrace::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Execute(TargetingHandle);
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);
	
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (!Ctx->SourceActor)
	{
		FHitResult HitResult;
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = HitResult;
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}
	
	FVector EyeLocation;
	FRotator EyeRotation;
	Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FVector EndLocation = EyeLocation + EyeRotation.Vector() * 10000.f;
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;
	if (AActor* Source = Cast<AActor>(Ctx->SourceObject))
	{
		Params.AddIgnoredActor(Source);
	}
	
	Ctx->InstigatorActor->GetWorld()->LineTraceSingleByChannel(HitResult, EyeLocation, EndLocation, ECC_Visibility, Params);
	
	FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
	ResultData->HitResult = HitResult;

	//DrawDebugLine(Ctx->InstigatorActor->GetWorld()
	//, EyeLocation, EndLocation
	//, FColor::Blue, false, 0.1, 0, 0.2f);
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}


UArcTargetingTask_LineTraceFromSocket::UArcTargetingTask_LineTraceFromSocket(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcTargetingTask_LineTraceFromSocket::Execute(const FTargetingRequestHandle& TargetingHandle) const
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
	USkeletalMeshComponent* SMC = Ctx->SourceActor->FindComponentByClass<USkeletalMeshComponent>();

	if (SMC == nullptr)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	
	FHitResult& CameraHit = HitSet->TargetResults[0].HitResult;

	FVector HitLoc = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : CameraHit.TraceEnd;
	FVector StartLocation = SMC->GetSocketLocation(SocketName);
	FTransform SocketTransform = SMC->GetSocketTransform(SocketName);

	//const FVector Direction = (HitLoc - StartLocation).GetSafeNormal();
	FVector Direction = SocketTransform.GetRotation().Rotator().Vector();
	//Direction = Ctx->SourceActor->GetTransform().InverseTransformVector(Direction);
	FVector EndLocation = StartLocation + (Direction * 10000.f);
	//FVector EndLocation = StartLocation + (StartLocation - HitLoc).GetSafeNormal() * 10000.f;

	//DrawDebugLine(Ctx->InstigatorActor->GetWorld()
	//	, StartLocation, EndLocation
	//	, FColor::Yellow, false, 0.1, 0, 2.f);
	
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Ctx->InstigatorActor.Get());
	Params.AddIgnoredActor(Ctx->SourceActor.Get());
	
	Ctx->InstigatorActor->GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, Params);
	HitSet->TargetResults.Empty();
		
	int32 Idx = HitSet->TargetResults.AddDefaulted();
	HitSet->TargetResults[Idx].HitResult = HitResult;
	
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}
