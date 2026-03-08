// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTT_ItemSphereTrace.h"

#include "ArcTargetingSourceContext.h"
#include "ArcTraceOrigin.h"
#include "ArcScalableFloatItemFragment_TargetingShape.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Items/ArcItemData.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "DrawDebugHelpers.h"

void UArcTT_ItemSphereTrace::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Execute(TargetingHandle);

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (!Ctx || !Ctx->SourceActor)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	FVector SourceLocation;

	const FArcTraceOrigin* Origin = TraceOriginOverride.GetPtr<FArcTraceOrigin>();
	if (Origin)
	{
		FVector Direction;
		bool bHasDirection = false;
		if (!Origin->Resolve(TargetingHandle, SourceLocation, Direction, bHasDirection))
		{
			SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
			return;
		}
	}
	else
	{
		SourceLocation = Ctx->SourceLocation;
		if (TargetingResults.TargetResults.Num() > 0)
		{
			const FHitResult& PrevHit = TargetingResults.TargetResults[0].HitResult;
			SourceLocation = PrevHit.bBlockingHit ? PrevHit.ImpactPoint : PrevHit.TraceEnd;
		}
	}

	float Radius = 200.f;
	if (const FArcItemData* Item = Ctx->SourceItemPtr)
	{
		Radius = FArcScalableFloatItemFragment_TargetingShape::GetRadiusValue(Item);
	}

	UWorld* World = Ctx->SourceActor->GetWorld();

	FCollisionQueryParams Params;
	Params.OwnerTag = GetFName();
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;

	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = ECollisionResponse::ECR_Overlap;

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, SourceLocation, SourceLocation + FVector(0, 0, 1),
		FQuat::Identity, CollisionChannel,
		FCollisionShape::MakeSphere(Radius), Params, ResponseParams);

	for (const FHitResult& Hit : HitResults)
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = Hit;
	}

#if ENABLE_DRAW_DEBUG
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		const float DrawTime = UTargetingSubsystem::GetOverrideTargetingLifeTime();
		DrawDebugSphere(World, SourceLocation, Radius, 16, FColor::Red, false, DrawTime, 0, 1.f);
		for (const FHitResult& Hit : HitResults)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, 8.f, 8, FColor::Green, false, DrawTime, 0, 1.f);
		}
	}
#endif

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

#if ENABLE_DRAW_DEBUG
void UArcTT_ItemSphereTrace::DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const
{
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
		FString DebugStr = FString::Printf(TEXT("ItemSphereTrace - Results: %d"), Results.TargetResults.Num());
		TargetingSubsystem->DebugLine(Info, DebugStr, XOffset, YOffset, MinTextRowsToAdvance);
	}
}
#endif
