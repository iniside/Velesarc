// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTT_ItemLineTrace.h"

#include "ArcTraceOrigin.h"
#include "ArcTargetingSourceContext.h"
#include "ArcScalableFloatItemFragment_TargetingTrace.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Items/ArcItemData.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "DrawDebugHelpers.h"

void UArcTT_ItemLineTrace::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Execute(TargetingHandle);
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (!Ctx || !Ctx->SourceActor)
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	float TraceDistance = 10000.f;
	if (const FArcItemData* Item = Ctx->SourceItemPtr)
	{
		TraceDistance = FArcScalableFloatItemFragment_TargetingTrace::GetDistanceValue(Item);
	}

	FVector StartLocation;
	FVector Direction;

	const FArcTraceOrigin* Origin = TraceOriginOverride.GetPtr<FArcTraceOrigin>();
	if (Origin)
	{
		bool bHasDirection = false;
		if (!Origin->Resolve(TargetingHandle, StartLocation, Direction, bHasDirection))
		{
			FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
			SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
			return;
		}
	}
	else
	{
		FRotator EyeRotation;
		Ctx->SourceActor->GetActorEyesViewPoint(StartLocation, EyeRotation);
		Direction = EyeRotation.Vector();
	}

	FVector EndLocation = StartLocation + Direction * TraceDistance;
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;
	if (AActor* Source = Cast<AActor>(Ctx->SourceObject))
	{
		Params.AddIgnoredActor(Source);
	}

	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);
	Ctx->SourceActor->GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, CollisionChannel, Params);

	FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
	ResultData->HitResult = HitResult;

#if ENABLE_DRAW_DEBUG
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		const float DrawTime = UTargetingSubsystem::GetOverrideTargetingLifeTime();
		UWorld* World = Ctx->SourceActor->GetWorld();
		DrawDebugLine(World, StartLocation, EndLocation, FColor::Red, false, DrawTime, 0, 1.f);
		if (HitResult.bBlockingHit)
		{
			DrawDebugSphere(World, HitResult.ImpactPoint, 8.f, 8, FColor::Green, false, DrawTime, 0, 1.f);
		}
	}
#endif

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

#if ENABLE_DRAW_DEBUG
void UArcTT_ItemLineTrace::DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const
{
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
		FString DebugStr = FString::Printf(TEXT("ItemLineTrace - Results: %d"), Results.TargetResults.Num());
		TargetingSubsystem->DebugLine(Info, DebugStr, XOffset, YOffset, MinTextRowsToAdvance);
	}
}
#endif
