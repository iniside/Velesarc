// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTT_ItemLineTrace.h"

#include "ArcTargetingSourceContext.h"
#include "ArcScalableFloatItemFragment_TargetingTrace.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Items/ArcItemData.h"

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

	FVector EyeLocation;
	FRotator EyeRotation;
	Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FVector EndLocation = EyeLocation + EyeRotation.Vector() * TraceDistance;
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;
	if (AActor* Source = Cast<AActor>(Ctx->SourceObject))
	{
		Params.AddIgnoredActor(Source);
	}

	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);
	Ctx->SourceActor->GetWorld()->LineTraceSingleByChannel(HitResult, EyeLocation, EndLocation, CollisionChannel, Params);

	FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
	ResultData->HitResult = HitResult;

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}
