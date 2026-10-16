// Copyright Lukasz Baran. All Rights Reserved.

#include "Functions/ArcStateTreeGetAbilitySocketLocationPropertyFunction.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "MassActorSubsystem.h"

void FArcStateTreeGetAbilitySocketLocationPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	InstanceData.Location = FVector::ZeroVector;

	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	const FMassEntityHandle Entity = AbilityContext.GetEntity();
	if (!EntityManager.IsEntityValid(Entity) || !EntityManager.IsEntityBuilt(Entity))
	{
		return;
	}

	const FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
	if (TransformFragment == nullptr)
	{
		return;
	}

	InstanceData.Location = TransformFragment->GetTransform().GetLocation();

	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (ActorFragment == nullptr || !ActorFragment->IsValid())
	{
		return;
	}

	AActor* Actor = ActorFragment->GetMutable();
	if (Actor == nullptr)
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (SkeletalMeshComponent != nullptr && SkeletalMeshComponent->DoesSocketExist(InstanceData.SocketName))
	{
		InstanceData.Location = SkeletalMeshComponent->GetSocketLocation(InstanceData.SocketName);
	}
}
