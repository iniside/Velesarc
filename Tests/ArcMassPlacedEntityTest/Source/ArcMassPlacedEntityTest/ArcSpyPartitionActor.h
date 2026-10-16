// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcSpyPartitionActor.generated.h"

/**
 * Minimal spy subclass of AArcPlacedEntityPartitionActor used exclusively by
 * OwnerObserver_FiresOnDestroy CQTest to verify that UArcPlacedEntityOwnerObserver
 * routes calls correctly without needing Entries/WorldPartition infrastructure.
 *
 * RecordRemovalForRespawn is overridden to capture {EntryIndex, TransformIndex} pairs
 * instead of running the real bookkeeping logic, which requires valid Entries arrays.
 * The spy deliberately does NOT call Super:: — the test validates routing, not bookkeeping.
 */
UCLASS(Hidden, NotPlaceable, NotBlueprintable)
class ASpyPartitionActor : public AArcPlacedEntityPartitionActor
{
	GENERATED_BODY()

public:
	struct FRecordedCall
	{
		int32 EntryIndex;
		int32 TransformIndex;
	};

	TArray<FRecordedCall> RecordedCalls;

	virtual void RecordRemovalForRespawn(int32 EntryIndex, int32 TransformIndex) override
	{
		FRecordedCall Call;
		Call.EntryIndex     = EntryIndex;
		Call.TransformIndex = TransformIndex;
		RecordedCalls.Add(Call);
	}
};
