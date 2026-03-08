// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTT_GroundForwardLocation.h"

#include "ArcTargetingSourceContext.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TargetingSystem/TargetingSubsystem.h"

void UArcTT_GroundForwardLocation::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (!Ctx || !Ctx->SourceActor)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	UWorld* World = Ctx->SourceActor->GetWorld();

	FRotator EyeRotation;
	FVector EyeLocation;
	Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FVector SourceLocation = Ctx->SourceActor->GetActorLocation();
	EyeRotation.Pitch = 0;
	FVector StartLocation = SourceLocation + EyeRotation.Vector() * Distance.GetValue();
	FVector EndLocation = StartLocation - FVector(0, 0, 400);

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

#if ENABLE_DRAW_DEBUG
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		const float DrawTime = UTargetingSubsystem::GetOverrideTargetingLifeTime();
		DrawDebugLine(World, StartLocation, EndLocation, FColor::Red, false, DrawTime, 0, 1.f);
		if (HitResult.bBlockingHit)
		{
			DrawDebugSphere(World, HitResult.ImpactPoint, 16, 16, FColor::Green, false, DrawTime, 0, 1.f);
		}
	}
#endif

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

#if ENABLE_DRAW_DEBUG
void UArcTT_GroundForwardLocation::DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const
{
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
		FString DebugStr = FString::Printf(TEXT("GroundForwardLocation - Results: %d"), Results.TargetResults.Num());
		TargetingSubsystem->DebugLine(Info, DebugStr, XOffset, YOffset, MinTextRowsToAdvance);
	}
}
#endif

void UArcTT_GroundForwardBox::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (!Ctx || !Ctx->SourceActor)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

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
	FVector BoxCenter = SourceLocation + (EyeRotation.Vector() * BoxHalfSize.X);

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, BoxCenter, BoxCenter + FVector(0, 0, 1),
		EyeRotation.Quaternion(), CollisionChannel, BoxShape, Params, ResponseParams);

	for (const FHitResult& Hit : HitResults)
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = Hit;
	}

#if ENABLE_DRAW_DEBUG
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		const float DrawTime = UTargetingSubsystem::GetOverrideTargetingLifeTime();
		DrawDebugBox(World, BoxCenter, BoxHalfSize, EyeRotation.Quaternion(), FColor::Red, false, DrawTime, 0, 1.f);
		for (const FHitResult& Hit : HitResults)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, 8.f, 8, FColor::Green, false, DrawTime, 0, 1.f);
		}
	}
#endif

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

#if ENABLE_DRAW_DEBUG
void UArcTT_GroundForwardBox::DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const
{
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
		FString DebugStr = FString::Printf(TEXT("GroundForwardBox - Results: %d"), Results.TargetResults.Num());
		TargetingSubsystem->DebugLine(Info, DebugStr, XOffset, YOffset, MinTextRowsToAdvance);
	}
}
#endif
