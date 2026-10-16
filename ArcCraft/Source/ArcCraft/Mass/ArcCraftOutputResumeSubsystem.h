// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Bags/ArcBagId.h"
#include "ArcCraftOutputResumeSubsystem.generated.h"

class UArcMassBagEventSubsystem;

/**
 * UArcCraftOutputResumeSubsystem
 *
 * Bridges bag-slot-freed events back to paused crafting stations. When the bag
 * delivery policy fails to place a crafted item (no accepting free slot), it
 * holds the output, pauses the station's active queue entry (bOutputBlocked),
 * and calls RegisterBlocked(BagOwner, Station). This subsystem listens on
 * UArcMassBagEventSubsystem::OnBagSlotsChanged; when a slot on a registered
 * BagOwner frees, it clears the blocked flag on the mapped station(s) and signals
 * ArcCraft.QueueChanged so the AutoTick processor re-attempts delivery of the
 * held output next tick.
 *
 * Drains on the game thread post-actor-tick (the bag event subsystem's drain),
 * so mutating the regular queue fragment here is safe — no Mass processor is
 * iterating at that point.
 */
UCLASS()
class ARCCRAFT_API UArcCraftOutputResumeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Records that crafting on Station is paused waiting on a free slot in BagOwner's bag.
	 *  Adds the (BagOwner, Station) pair only if not already present. Called from the
	 *  bag delivery policy's deferred command (Step 2). */
	void RegisterBlocked(FMassEntityHandle BagOwner, FMassEntityHandle Station);

	/** Despawn cleanup: removes Entity both as a key and from every value list. */
	void EvictEntity(FMassEntityHandle Entity);

private:
	void OnBagSlotsChanged(FMassEntityHandle BagOwner, FArcBagId BagId);

	/** BagOwner entity -> blocked station entities waiting on it to free a slot. */
	TMultiMap<FMassEntityHandle, FMassEntityHandle> BlockedByBagOwner;

	UArcMassBagEventSubsystem* BagEventSubsystem = nullptr;
	FDelegateHandle BagSlotsChangedHandle;
};
