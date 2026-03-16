// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTT_ItemBoxTrace.h"

#include "ArcTargetingSourceContext.h"
#include "ArcTraceOrigin.h"
#include "ArcAoETypes.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Items/ArcItemData.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "DrawDebugHelpers.h"

void UArcTT_ItemBoxTrace::Execute(const FTargetingRequestHandle& TargetingHandle) const
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
	FVector Direction;

	const FArcTraceOrigin* Origin = TraceOriginOverride.GetPtr<FArcTraceOrigin>();
	if (Origin)
	{
		bool bHasDirection = false;
		if (!Origin->Resolve(TargetingHandle, SourceLocation, Direction, bHasDirection))
		{
			SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
			return;
		}
	}
	else
	{
		if (TargetingResults.TargetResults.IsEmpty())
		{
			SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
			return;
		}
		SourceLocation = TargetingResults.TargetResults[0].HitResult.ImpactPoint;

		FVector EyeLocation;
		FRotator EyeRotation;
		Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);
		Direction = EyeRotation.Vector();
	}

	FArcAoEShapeData ShapeData;
	if (const FArcItemData* Item = Ctx->SourceItemPtr)
	{
		ShapeData = FArcAoEShapeData::FromItemData(Item);
	}

	FVector BoxCenter;
	FQuat BoxRotation;
	ShapeData.ComputeBoxTransform(SourceLocation, Direction, BoxCenter, BoxRotation);

	UWorld* World = Ctx->SourceActor->GetWorld();

	FCollisionQueryParams Params;
	Params.OwnerTag = GetFName();
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;

	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = ECollisionResponse::ECR_Overlap;

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, BoxCenter, BoxCenter + FVector(0, 0, 1),
		BoxRotation, CollisionChannel,
		FCollisionShape::MakeBox(ShapeData.BoxExtent), Params, ResponseParams);

	for (const FHitResult& Hit : HitResults)
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = Hit;
	}

#if ENABLE_DRAW_DEBUG
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		const float DrawTime = UTargetingSubsystem::GetOverrideTargetingLifeTime();
		DrawDebugBox(World, BoxCenter, ShapeData.BoxExtent, BoxRotation, FColor::Red, false, DrawTime, 0, 1.f);
		for (const FHitResult& Hit : HitResults)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, 8.f, 8, FColor::Green, false, DrawTime, 0, 1.f);
		}
	}
#endif

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

#if ENABLE_DRAW_DEBUG
void UArcTT_ItemBoxTrace::DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const
{
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
		FString DebugStr = FString::Printf(TEXT("ItemBoxTrace - Results: %d"), Results.TargetResults.Num());
		TargetingSubsystem->DebugLine(Info, DebugStr, XOffset, YOffset, MinTextRowsToAdvance);
	}
}
#endif
