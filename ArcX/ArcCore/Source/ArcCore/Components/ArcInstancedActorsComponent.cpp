// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedActorsComponent.h"

#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"

// Sets default values for this component's properties
UArcInstancedActorsComponent::UArcInstancedActorsComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

// Called when the game starts
void UArcInstancedActorsComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


void UArcInstancedActorsComponent::ModifyMassEntityConfig(FMassEntityManager& InMassEntityManager, UInstancedActorsData* InstancedActorData
	, FMassEntityConfig& InOutMassEntityConfig) const
{
	for (UMassEntityTraitBase* Trait : Traits)
	{
		//if (Trait && !InOutMassEntityConfig.FindTrait(Trait->GetClass(), true))
		{
			InOutMassEntityConfig.AddTrait(*Trait);
		}
	}
}


void UArcInstancedActorsComponent::OnServerPreSpawnInitForInstance(FInstancedActorsInstanceHandle InInstanceHandle)
{
	Super::OnServerPreSpawnInitForInstance(InInstanceHandle);
}

void UArcInstancedActorsComponent::InitializeComponentForInstance(FInstancedActorsInstanceHandle InInstanceHandle)
{
	Super::InitializeComponentForInstance(InInstanceHandle);
}
