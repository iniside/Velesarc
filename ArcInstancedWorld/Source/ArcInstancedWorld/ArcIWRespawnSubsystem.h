// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcIWRespawnSubsystem.generated.h"

class AArcIWMassISMPartitionActor;

/**
 * Tickable world subsystem that periodically checks loaded partition actors
 * for expired removal entries and restores (respawns) those entities.
 */
UCLASS()
class ARCINSTANCEDWORLD_API UArcIWRespawnSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RegisterPartitionActor(AArcIWMassISMPartitionActor* Actor);
	void UnregisterPartitionActor(AArcIWMassISMPartitionActor* Actor);

private:
	/** All currently loaded partition actors. */
	TArray<TWeakObjectPtr<AArcIWMassISMPartitionActor>> RegisteredPartitions;

	/** Accumulated time since last sweep. */
	float TimeSinceLastSweep = 0.f;

	/** Sweep interval in seconds. */
	static constexpr float SweepInterval = 1.0f;
};
