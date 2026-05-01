// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcStrategyTrainingCoordinator.generated.h"

class ULearningAgentsManager;

/**
 * Hosts two ULearningAgentsManager components — one for the settlement agent
 * population, one for factions. Spawned by UArcStrategyTrainingSubsystem
 * during training setup.
 */
UCLASS(NotPlaceable)
class ARCECONOMY_API AArcStrategyTrainingCoordinator : public AActor
{
	GENERATED_BODY()

public:
	AArcStrategyTrainingCoordinator();

	UPROPERTY()
	TObjectPtr<ULearningAgentsManager> SettlementManager;

	UPROPERTY()
	TObjectPtr<ULearningAgentsManager> FactionManager;
};
