// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcNavMoverComponent.h"

#include "MoverComponent.h"
#include "Mover/ArcWalkGait.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNavMoverComponent)

float UArcNavMoverComponent::GetMaxSpeedForNavMovement() const
{
	if (const UMoverComponent* Mover = MoverComponent.Get())
	{
		if (const IArcWalkGaitProvider* Provider = ArcMover::FindWalkGaitProvider(*Mover))
		{
			return Provider->GetBaseMaxSpeed();
		}
	}

	return Super::GetMaxSpeedForNavMovement();
}
