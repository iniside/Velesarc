// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcIWEditorCommands.generated.h"

struct FArcIWMeshEntry;
class AActor;
class AArcIWMassISMPartitionActor;
class APartitionActor;
class UBodySetup;

namespace UE::ArcIW::Editor
{
	/**
	 * Captures ALL mesh data from an actor: static meshes (including individual ISMC instances)
	 * and skinned meshes. Unlike ArcMass::CompositeVis::CaptureActorMeshData, this version
	 * does NOT exclude ISMCs and also handles skinned mesh components.
	 */
	ARCINSTANCEDWORLDEDITOR_API void CaptureActorMeshData(AActor* Actor, TArray<FArcIWMeshEntry>& OutEntries);

	/**
	 * Merges collision shape data from all collision-enabled primitive components of the actor CDO.
	 * OutBodySetup is null if the actor class has no collision-enabled components.
	 */
	ARCINSTANCEDWORLDEDITOR_API void CaptureActorCollisionData(
		TSubclassOf<AActor> ActorClass,
		UObject* Outer,
		UBodySetup*& OutBodySetup,
		FCollisionProfileName& OutCollisionProfile,
		FBodyInstance& OutBodyTemplate);

	/** Overload that captures from an existing actor instance instead of the CDO.
	 *  Use this for Blueprint classes whose collision components are added in the SCS. */
	ARCINSTANCEDWORLDEDITOR_API void CaptureActorCollisionData(
		AActor* Actor,
		UObject* Outer,
		UBodySetup*& OutBodySetup,
		FCollisionProfileName& OutCollisionProfile,
		FBodyInstance& OutBodyTemplate);

	/**
	 * Converts selected actors into partition-based representation.
	 * Mesh data is captured, actors are added to AArcIWMassISMPartitionActor cells, and originals are destroyed.
	 */
	void ConvertActorsToPartition(UWorld* World, const TArray<AActor*>& Actors);

	/**
	 * Finds an existing AArcIWMassISMPartitionActor for the given location and grid,
	 * or creates a new one if none exists.
	 */
	ARCINSTANCEDWORLDEDITOR_API AArcIWMassISMPartitionActor* FindOrCreatePartitionActor(
		UWorld* World,
		const FVector& WorldLocation,
		FName GridName);

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
