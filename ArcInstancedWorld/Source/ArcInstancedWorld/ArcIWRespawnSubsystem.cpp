// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWRespawnSubsystem.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"

void UArcIWRespawnSubsystem::Tick(float DeltaTime)
{
	TimeSinceLastSweep += DeltaTime;
	if (TimeSinceLastSweep < SweepInterval)
	{
		return;
	}
	TimeSinceLastSweep = 0.f;

	int64 NowUtc = FDateTime::UtcNow().ToUnixTimestamp();

	for (int32 Idx = RegisteredPartitions.Num() - 1; Idx >= 0; --Idx)
	{
		AArcIWMassISMPartitionActor* Actor = RegisteredPartitions[Idx].Get();
		if (!Actor)
		{
			RegisteredPartitions.RemoveAtSwap(Idx);
			continue;
		}

		Actor->RestoreExpiredRemovals(NowUtc);
	}
}

TStatId UArcIWRespawnSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcIWRespawnSubsystem, STATGROUP_Tickables);
}

void UArcIWRespawnSubsystem::RegisterPartitionActor(AArcIWMassISMPartitionActor* Actor)
{
	if (Actor)
	{
		RegisteredPartitions.AddUnique(Actor);
	}
}

void UArcIWRespawnSubsystem::UnregisterPartitionActor(AArcIWMassISMPartitionActor* Actor)
{
	RegisteredPartitions.RemoveSingleSwap(Actor);
}
