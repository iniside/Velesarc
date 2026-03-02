// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/ArcUtilityTypes.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"

FVector FArcUtilityTarget::GetLocation(const FMassEntityManager* EntityManager) const
{
	switch (TargetType)
	{
	case EArcUtilityTargetType::Actor:
		if (AActor* ActorPtr = Actor.Get())
		{
			return ActorPtr->GetActorLocation();
		}
		break;

	case EArcUtilityTargetType::Entity:
		if (EntityManager && EntityManager->IsEntityValid(EntityHandle))
		{
			const FTransformFragment* TransformFragment = EntityManager->GetFragmentDataPtr<FTransformFragment>(EntityHandle);
			if (TransformFragment)
			{
				return TransformFragment->GetTransform().GetLocation();
			}
		}
		break;

	case EArcUtilityTargetType::Location:
		return Location;

	default:
		break;
	}

	return FVector::ZeroVector;
}

AActor* FArcUtilityTarget::GetActor(const FMassEntityManager* EntityManager) const
{
	switch (TargetType)
	{
	case EArcUtilityTargetType::Actor:
		return Actor.Get();

	case EArcUtilityTargetType::Entity:
		if (EntityManager && EntityManager->IsEntityValid(EntityHandle))
		{
			FMassActorFragment* ActorFragment = EntityManager->GetFragmentDataPtr<FMassActorFragment>(EntityHandle);
			if (ActorFragment)
			{
				return ActorFragment->GetMutable();
			}
		}
		break;

	default:
		break;
	}

	return nullptr;
}
