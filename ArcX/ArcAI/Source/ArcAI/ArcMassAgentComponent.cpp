// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAgentComponent.h"

#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"

// Sets default values for this component's properties
UArcMassAgentComponent::UArcMassAgentComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void UArcMassAgentComponent::SetEntityHandle(const FMassEntityHandle NewHandle)
{
	Super::SetEntityHandle(NewHandle);

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld());
	
	const FMassEntityView EntityView(EntitySubsystem->GetEntityManager(), AgentHandle);
	FTransformFragment* TransformFragment = EntityView.GetFragmentDataPtr<FTransformFragment>();
	if (TransformFragment)
	{
		TransformFragment->SetTransform(GetOwner()->GetActorTransform());
	}
}
