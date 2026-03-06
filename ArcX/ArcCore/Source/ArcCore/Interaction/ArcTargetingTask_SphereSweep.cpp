// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetingTask_SphereSweep.h"

#include "Targeting/ArcTargetingSourceContext.h"
#include "Types/TargetingSystemTypes.h"
#include "Engine/World.h"

void UArcTargetingTask_SphereSweep::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	FVector SourceLocation = Ctx->SourceActor->GetActorLocation();
	UWorld* World = Ctx->SourceActor->GetWorld();

	FVector StartLocation = SourceLocation;
	FVector EndLocation = StartLocation + FVector(0,0,1);

	FCollisionQueryParams Params;
	Params.OwnerTag = GetFName();
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;

	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = ECollisionResponse::ECR_Overlap;

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, StartLocation, EndLocation, FQuat::Identity, CollisionChannel,
		FCollisionShape::MakeSphere(Radius.GetValue()), Params, ResponseParams);

	for (const FHitResult& Hit : HitResults)
	{
		FTargetingDefaultResultData* NewResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		NewResultData->HitResult = Hit;
	}
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}
