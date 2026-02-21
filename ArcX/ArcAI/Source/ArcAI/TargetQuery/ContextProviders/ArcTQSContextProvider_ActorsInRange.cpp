// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSContextProvider_ActorsInRange.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"

void FArcTQSContextProvider_ActorsInRange::GenerateContextLocations(
	const FArcTQSQueryContext& QueryContext,
	TArray<FVector>& OutLocations) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return;
	}

	const FVector Center = QueryContext.QuerierLocation;
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTQSActorsInRange));
	if (bExcludeQuerier)
	{
		if (const AActor* QuerierActor = QueryContext.QuerierActor.Get())
		{
			Params.AddIgnoredActor(QuerierActor);
		}
	}

	// Use object type query matching the specified collision channel
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(CollisionChannel);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, SphereShape, Params);

	// Deduplicate by actor and optionally filter by class
	TSet<AActor*> SeenActors;
	OutLocations.Reserve(OutLocations.Num() + Overlaps.Num());

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		if (!OverlapActor)
		{
			continue;
		}

		// Class filter
		if (ActorClassFilter && !OverlapActor->IsA(ActorClassFilter))
		{
			continue;
		}

		bool bAlreadyInSet = false;
		SeenActors.Add(OverlapActor, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			continue;
		}

		OutLocations.Add(OverlapActor->GetActorLocation());
	}
}
