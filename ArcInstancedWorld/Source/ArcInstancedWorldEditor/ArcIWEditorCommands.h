// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcIWEditorCommands.generated.h"

struct FArcIWMeshEntry;
class AArcIWMassISMPartitionActor;
class APartitionActor;

namespace UE::ArcIW::Editor
{
	/**
	 * Captures ALL static mesh data from an actor, including individual ISMC instances.
	 * Unlike ArcMass::CompositeVis::CaptureActorMeshData, this version does NOT exclude ISMCs.
	 */
	void CaptureActorMeshData(AActor* Actor, TArray<FArcIWMeshEntry>& OutEntries);

	/**
	 * Converts selected actors into partition-based representation.
	 * Mesh data is captured, actors are added to AArcIWMassISMPartitionActor cells, and originals are destroyed.
	 */
	void ConvertActorsToPartition(UWorld* World, const TArray<AActor*>& Actors);

	/**
	 * Restores actors from a partition actor, spawning one actor per stored transform,
	 * then destroys the partition actor.
	 */
	void ConvertPartitionToActors(UWorld* World, AArcIWMassISMPartitionActor* PartitionActor);
}

/**
 * Blueprint-callable wrappers for ArcIW editor conversion commands.
 */
UCLASS()
class UArcIWEditorCommandLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Converts the given actors into partition representation. */
	UFUNCTION(BlueprintCallable, Category = "ArcIW|Editor", meta = (WorldContext = "WorldContextObject"))
	static void ConvertActorsToPartition(UObject* WorldContextObject, const TArray<AActor*>& Actors);

	/** Restores actors from a partition actor. */
	UFUNCTION(BlueprintCallable, Category = "ArcIW|Editor", meta = (WorldContext = "WorldContextObject"))
	static void ConvertPartitionToActors(UObject* WorldContextObject, APartitionActor* PartitionActor);
};
