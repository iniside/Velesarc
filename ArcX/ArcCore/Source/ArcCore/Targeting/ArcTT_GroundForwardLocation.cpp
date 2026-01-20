// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTT_GroundForwardLocation.h"

#include "ArcTargetingSourceContext.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UArcTT_GroundForwardLocation::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);
	
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	FVector SourceLocation;
	UWorld* World = Ctx->SourceActor->GetWorld();

	FRotator EyeRotation;
	FVector EyeLocation;
	Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	
	SourceLocation = Ctx->SourceActor->GetActorLocation();
	EyeRotation.Pitch = 0;
	FVector StartLocation = SourceLocation + EyeRotation.Vector() * Distance.GetValue();
	FVector EndLocation = StartLocation - FVector(0,0,400);
	
	FCollisionQueryParams Params;
	Params.OwnerTag = GetFName();
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;
	
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = ECollisionResponse::ECR_Block;

	FHitResult HitResult;
	World->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, CollisionChannel, Params, ResponseParams);
	
	
	FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
	ResultData->HitResult = HitResult;
	
	DrawDebugLine(World, StartLocation, EndLocation, FColor::Yellow, false, -1, 0, 1.f);
	DrawDebugSphere(World, HitResult.ImpactPoint, 16, 32, FColor::Yellow, false, -1, 0, 1.f);
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

void UArcTT_GroundForwardBox::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (TargetingResults.TargetResults.IsEmpty())
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}
	
	FVector SourceLocation = TargetingResults.TargetResults[0].HitResult.ImpactPoint;
	UWorld* World = Ctx->SourceActor->GetWorld();

	FRotator EyeRotation;
	FVector EyeLocation;
	Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	
	EyeRotation.Pitch = 0;

	
	FCollisionQueryParams Params;
	Params.OwnerTag = GetFName();
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;
	
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = ECollisionResponse::ECR_Block;
	
	FVector BoxHalfSize(Distance.GetValue() * 0.5f, 100.f, 100.f); 
	FCollisionShape BoxShape = FCollisionShape::MakeBox(BoxHalfSize);
	//FVector BoxCenter = SourceLocation + (EyeRotation.Vector() * Distance.GetValue());// * 0.5f);
	FVector BoxCenter = SourceLocation + (EyeRotation.Vector() * BoxHalfSize.X);
	
	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, BoxCenter, BoxCenter + FVector(0,0,1), EyeRotation.Quaternion(), CollisionChannel, BoxShape, Params, ResponseParams);
	
	DrawDebugBox(World, BoxCenter, BoxHalfSize, EyeRotation.Quaternion(), FColor::Yellow, false, -1, 0, 1.f);
	
	for (const FHitResult& Hit : HitResults)
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = Hit;
	}
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}
