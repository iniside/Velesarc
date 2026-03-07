// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTT_ItemSphereTrace.h"

#include "ArcTargetingSourceContext.h"
#include "ArcScalableFloatItemFragment_TargetingShape.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Items/ArcItemData.h"

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

	FVector SourceLocation = Ctx->SourceLocation;
	if (TargetingResults.TargetResults.Num() > 0)
	{
		const FHitResult& PrevHit = TargetingResults.TargetResults[0].HitResult;
		SourceLocation = PrevHit.bBlockingHit ? PrevHit.ImpactPoint : PrevHit.TraceEnd;
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

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}
