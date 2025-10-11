// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAIPerceptionComponent.h"

// Sets default values for this component's properties
UArcAIPerceptionComponent::UArcAIPerceptionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

// Called when the game starts
void UArcAIPerceptionComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	OnPerceptionUpdated.AddDynamic(this, &UArcAIPerceptionComponent::HandleOnPerceptionUpdated);
	OnTargetPerceptionForgotten.AddDynamic(this, &UArcAIPerceptionComponent::HandleOnTargetPerceptionForgotten);
	OnTargetPerceptionUpdated.AddDynamic(this, &UArcAIPerceptionComponent::HandleOnTargetPerceptionUpdated);
	OnTargetPerceptionInfoUpdated.AddDynamic(this, &UArcAIPerceptionComponent::HandleOnTargetPerceptionInfoUpdated);
}

void UArcAIPerceptionComponent::HandleOnPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
	NativeOnPerceptionUpdated.Broadcast(UpdatedActors);
	NativeOnPerceptionChanged.Broadcast();
}

void UArcAIPerceptionComponent::HandleOnTargetPerceptionForgotten(AActor* Actor)
{
	NativeOnTargetPerceptionForgotten.Broadcast(Actor);
	NativeOnPerceptionChanged.Broadcast();
}

void UArcAIPerceptionComponent::HandleOnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	NativeOnTargetPerceptionUpdated.Broadcast(Actor, Stimulus);
	NativeOnPerceptionChanged.Broadcast();
}

void UArcAIPerceptionComponent::HandleOnTargetPerceptionInfoUpdated(const FActorPerceptionUpdateInfo& UpdateInfo)
{
	NativeOnTargetPerceptionInfoUpdated.Broadcast(UpdateInfo);
	NativeOnPerceptionChanged.Broadcast();
}
