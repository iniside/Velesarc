// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWEntityComponent.h"
#include "ArcIWTypes.h"
#include "MassEntitySubsystem.h"

void UArcIWEntityComponent::SetKeepHydrated(bool bKeep)
{
	if (!EntityHandle.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(EntityHandle))
	{
		return;
	}

	FArcIWInstanceFragment* Instance = EntityManager.GetFragmentDataPtr<FArcIWInstanceFragment>(EntityHandle);
	if (Instance)
	{
		Instance->bKeepHydrated = bKeep;
	}
}

bool UArcIWEntityComponent::IsKeepHydrated() const
{
	if (!EntityHandle.IsValid())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return false;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(EntityHandle))
	{
		return false;
	}

	const FArcIWInstanceFragment* Instance = EntityManager.GetFragmentDataPtr<FArcIWInstanceFragment>(EntityHandle);
	return Instance ? Instance->bKeepHydrated : false;
}
