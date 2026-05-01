// Copyright Lukasz Baran. All Rights Reserved.

#include "ML/ArcStrategyTrainingCoordinator.h"
#include "LearningAgentsManager.h"

AArcStrategyTrainingCoordinator::AArcStrategyTrainingCoordinator()
{
	PrimaryActorTick.bCanEverTick = false;

	SettlementManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("SettlementManager"));
	FactionManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("FactionManager"));
}
