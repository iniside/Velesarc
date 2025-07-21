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



#include "ArcTT_ConeTrace.h"

#include "ArcTargetingSourceContext.h"
#include "CollisionDebugDrawingPublic.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UArcTT_ConeTrace::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Execute(TargetingHandle);
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);
	
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	UWorld* World = Ctx->SourceActor->GetWorld();

	FVector EyeLocation;
	FRotator EyeRotation;
	Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FVector::FReal DistanceToActor = 0;
	if (bOffsetConeToActorEyes)
	{
		DistanceToActor = FVector::Dist(EyeLocation, Ctx->SourceActor->GetActorLocation());
	}
	
	FVector StartLocation = (EyeRotation.Vector() * DistanceToActor) + EyeLocation;
	FVector EndLocation = StartLocation + EyeRotation.Vector() * ConeLength;
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.OwnerTag = GetFName();
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;
	
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = ECollisionResponse::ECR_Overlap;

	World->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, Params);

	FHitResult HitFromSource;
	{
		FVector FromActorStartLocation = Ctx->SourceActor->GetActorLocation();
		FVector FromActorEndLocation = FromActorStartLocation + Ctx->SourceActor->GetActorForwardVector() * FromSourceActorLength;
		World->SweepSingleByChannel(HitFromSource, FromActorStartLocation, FromActorEndLocation, FQuat::Identity, ECC_Visibility
			, FCollisionShape::MakeSphere(32.f), Params);
	}
	
	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, StartLocation, EndLocation, EyeRotation.Quaternion(), CollisionChannel,
		FCollisionShape::MakeSphere(300.f), Params, ResponseParams);

	TArray<FHitResult> FilteredHitResults;
	const float ConeRad = FMath::DegreesToRadians(ConeAngle);

	for (const FHitResult& Hit : HitResults)
	{
		if (Hit.GetActor() && Hit.GetActor()->GetClass()->IsChildOf(ActorClass))
		{
			const FVector ToTarget = (Hit.ImpactPoint - StartLocation).GetSafeNormal();
			const float Dot = FVector::DotProduct(ToTarget, EyeRotation.Vector());
			const float AngleToTarget = FMath::Acos(Dot);

			const FVector::FReal Dist = FVector::Dist(Hit.ImpactPoint, StartLocation);
			if (Dist < IgnoreCodeAngleDistance)
			{
				if (Dot > 0.0f)
				{
					FilteredHitResults.Add(Hit);
					continue;
				}
			}
			
			if (ConeRad > AngleToTarget)
			{
				FilteredHitResults.Add(Hit);
			}	
		}
	}

	APawn* P = Cast<APawn>(Ctx->SourceActor);
	APlayerController* PC = P ? P->GetController<APlayerController>() : nullptr;
	if (!PC)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}
	int32 ScreenWidth, ScreenHeight;
	PC->GetViewportSize(ScreenWidth, ScreenHeight);

	// Calculate the screen-space center
	FVector2D ScreenCenter = FVector2D(ScreenWidth / 2.0f, ScreenHeight / 2.0f);
	
	int32 BestHit = INDEX_NONE;

	float BestAlignment = -1.f;
	float BestDistance = FLT_MAX;

	float BestDistanceToActor = FLT_MAX;
	int32 BestDistanceHit = INDEX_NONE;
	float BestAlignmentDist = -1.f;
	for (int32 HitIndex = 0; HitIndex < FilteredHitResults.Num(); ++HitIndex)
	{
		const FHitResult& Hit = FilteredHitResults[HitIndex];
		FVector ToTarget = (Hit.GetActor()->GetActorLocation() - StartLocation).GetSafeNormal();
		const float Alignment = FVector::DotProduct(ToTarget, EyeRotation.Vector());
		if (Alignment > BestAlignment)
		{
			BestAlignment = Alignment;
			BestHit = HitIndex;
		}

		const float DistSqr = FVector::DistSquared(Hit.ImpactPoint, Ctx->SourceActor->GetActorLocation());
		if ( (DistSqr <= FilterPrioritizeDistance*FilterPrioritizeDistance) && DistSqr <= BestDistanceToActor)
		{
			FVector ToTarget2 = (Hit.GetActor()->GetActorLocation() - Ctx->SourceActor->GetActorLocation()).GetSafeNormal();
			const float Alignment2 = FVector::DotProduct(ToTarget2, EyeRotation.Vector());
			if (Alignment2 > BestAlignmentDist)
			{
				BestAlignmentDist = Alignment2;
				BestDistance = DistSqr;
				BestDistanceHit = HitIndex;	
			}
		}
	}
	if (HitFromSource.GetActor() && HitFromSource.GetActor()->GetClass()->IsChildOf(ActorClass))
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = HitFromSource;
		DrawDebugSphere(World, HitFromSource.ImpactPoint + FVector(0,0, -10), 16.f, 8, FColor::Yellow, false, 0.1, 0, 0.2f);
	}
	else if (HitResult.GetActor() && HitResult.GetActor()->GetClass()->IsChildOf(ActorClass))
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = HitResult;
		DrawDebugSphere(World, HitResult.ImpactPoint + FVector(0,0, -10), 16.f, 8, FColor::Yellow, false, 0.1, 0, 0.2f);
	}
	else if (BestDistanceHit != INDEX_NONE)
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = FilteredHitResults[BestDistanceHit];
		DrawDebugSphere(World, FilteredHitResults[BestDistanceHit].ImpactPoint + FVector(0,0, -10), 16.f, 8, FColor::Yellow, false, 0.1, 0, 0.2f);
	}
	else if (BestHit != INDEX_NONE)
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = FilteredHitResults[BestHit];
		DrawDebugSphere(World, FilteredHitResults[BestHit].ImpactPoint + FVector(0,0, -10), 16.f, 8, FColor::Yellow, false, 0.1, 0, 0.2f);
	}
	else
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		HitResult.ImpactPoint = HitResult.TraceEnd;
		HitResult.Location = HitResult.TraceEnd;
		ResultData->HitResult = HitResult;
	}
	//DrawSphereSweeps(World, StartLocation, EndLocation, 20.f, HitResults, 0.5f);
	for (const FHitResult& Hit : HitResults)
	{
		DrawDebugSphere(World, Hit.ImpactPoint, 16.f, 8, FColor::Red, false, 0.1, 0, 0.2f);
	}

	for (const FHitResult& Hit : FilteredHitResults)
	{
		DrawDebugSphere(World, Hit.ImpactPoint + FVector(0,0, 10), 16.f, 8, FColor::Blue, false, 0.1, 0, 0.2f);
	}
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);

	
	DrawDebugCone(World, StartLocation, EyeRotation.Vector(), ConeLength, ConeRad, ConeRad
		, 12, FColor::Red, false, 0.1, 0, 0.2f);
}
