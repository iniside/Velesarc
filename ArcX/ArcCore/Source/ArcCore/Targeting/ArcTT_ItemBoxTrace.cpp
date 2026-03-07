// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTT_ItemBoxTrace.h"

#include "ArcTargetingSourceContext.h"
#include "ArcAoETypes.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Items/ArcItemData.h"

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

	if (TargetingResults.TargetResults.IsEmpty())
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	FVector SourceLocation = TargetingResults.TargetResults[0].HitResult.ImpactPoint;

	FArcAoEShapeData ShapeData;
	if (const FArcItemData* Item = Ctx->SourceItemPtr)
	{
		ShapeData = FArcAoEShapeData::FromItemData(Item);
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FVector BoxCenter;
	FQuat BoxRotation;
	ShapeData.ComputeBoxTransform(SourceLocation, EyeRotation.Vector(), BoxCenter, BoxRotation);

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

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}
