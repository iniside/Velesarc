// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRecoilEntityResolve.h"

#include "Engine/World.h"
#include "Fragments/ArcRecoilFragments.h"
#include "GameFramework/Actor.h"
#include "MassAgentComponent.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

ArcRecoil::EEntityResolveResult ArcRecoil::ResolveRecoilEntity(const AActor* Actor, FResolvedRecoilEntity& Out)
{
	Out = FResolvedRecoilEntity();

	if (Actor == nullptr)
	{
		return EEntityResolveResult::NoActor;
	}

	UWorld* World = Actor->GetWorld();
	if (World == nullptr)
	{
		return EEntityResolveResult::NoWorld;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return EEntityResolveResult::NoEntitySubsystem;
	}

	const UMassAgentComponent* AgentComponent = Actor->FindComponentByClass<UMassAgentComponent>();
	if (AgentComponent == nullptr)
	{
		return EEntityResolveResult::NoAgentComponent;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	const FMassEntityHandle Entity = AgentComponent->GetEntityHandle();
	if (!Entity.IsSet() || !EntityManager.IsEntityActive(Entity))
	{
		return EEntityResolveResult::InvalidHandle;
	}

	if (EntityManager.GetFragmentDataPtr<FArcRecoilStateFragment>(Entity) == nullptr
		|| EntityManager.GetFragmentDataPtr<FArcRecoilProfileFragment>(Entity) == nullptr)
	{
		return EEntityResolveResult::MissingFragments;
	}

	Out.EntityManager = &EntityManager;
	Out.Entity = Entity;

	return EEntityResolveResult::Resolved;
}

const TCHAR* ArcRecoil::DescribeResolveResult(const EEntityResolveResult Result)
{
	switch (Result)
	{
	case EEntityResolveResult::Resolved:
		return TEXT("resolved");
	case EEntityResolveResult::NoActor:
		return TEXT("no actor");
	case EEntityResolveResult::NoWorld:
		return TEXT("actor has no world");
	case EEntityResolveResult::NoEntitySubsystem:
		return TEXT("world has no UMassEntitySubsystem");
	case EEntityResolveResult::NoAgentComponent:
		return TEXT("actor has no UMassAgentComponent");
	case EEntityResolveResult::InvalidHandle:
		return TEXT("UMassAgentComponent handle is not an active entity");
	case EEntityResolveResult::MissingFragments:
		return TEXT("entity has no ArcRecoil fragments (missing UArcRecoilTrait)");
	default:
		return TEXT("unknown");
	}
}
