// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSTypes.h"
#include "MassActorSubsystem.h"
#include "MassEntityFragments.h"
#include "MassEntityManager.h"
#include "GameFramework/Actor.h"

FVector FArcTQSTargetItem::GetLocation(const FMassEntityManager* EntityManager) const
{
	switch (TargetType)
	{
	case EArcTQSTargetType::Location:
		return Location;

	case EArcTQSTargetType::Actor:
		if (const AActor* ActorPtr = Actor.Get())
		{
			return ActorPtr->GetActorLocation();
		}
		return Location;

	case EArcTQSTargetType::MassEntity:
		if (EntityManager && EntityManager->IsEntityValid(EntityHandle))
		{
			if (const FTransformFragment* TransformFrag = EntityManager->GetFragmentDataPtr<FTransformFragment>(EntityHandle))
			{
				return TransformFrag->GetTransform().GetLocation();
			}
		}
		return Location;

	default:
		return Location;
	}
}

AActor* FArcTQSTargetItem::GetActor(const FMassEntityManager* EntityManager) const
{
	if (Actor.IsValid())
	{
		return Actor.Get();
	}

	if (EntityManager && TargetType == EArcTQSTargetType::MassEntity && EntityManager->IsEntityValid(EntityHandle))
	{
		if (const FMassActorFragment* ActorFrag = EntityManager->GetFragmentDataPtr<FMassActorFragment>(EntityHandle))
		{
			return const_cast<AActor*>(ActorFrag->Get());
		}
	}

	return nullptr;
}

FMassEntityHandle FArcTQSTargetItem::GetEntityHandle(const FMassEntityManager* EntityManager) const
{
	if (EntityHandle.IsValid())
	{
		return EntityHandle;
	}

	// Could potentially resolve actor to entity via UMassActorSubsystem, but that requires a world context
	// For now, return invalid if we only have an actor
	return FMassEntityHandle();
}
