// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcIWBlueprintLibrary.generated.h"

/**
 * Blueprint utility functions for ArcInstancedWorld.
 */
UCLASS()
class ARCINSTANCEDWORLD_API UArcIWBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Resolve a trace hit result to the Mass entity it belongs to.
	 * Works for hits on ArcIW ISM components. Returns invalid handle for non-ArcIW hits.
	 * If bHydrateOnHit is enabled in project settings, also spawns the actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcIW", meta = (WorldContext = "WorldContextObject"))
	static FMassEntityHandle ResolveHitToEntity(UObject* WorldContextObject, const FHitResult& HitResult);

	/**
	 * Force-hydrate a specific entity — remove ISM representation and spawn the real actor.
	 * No-op if entity is already in actor representation.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcIW", meta = (WorldContext = "WorldContextObject"))
	static void HydrateEntity(UObject* WorldContextObject, FMassEntityHandle EntityHandle);
};
