// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsForce.h"
#include "ArcMassPhysicsSimulation.h"
#include "MassEntityManager.h"
#include "PhysicsEngine/BodyInstance.h"

namespace ArcMassPhysicsForce
{
	namespace Internal
	{
		void EnableSimulation(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FBodyInstance& Body)
		{
			if (!Body.IsInstanceSimulatingPhysics())
			{
				Body.SetInstanceSimulatePhysics(true);
			}

			EntityManager.AddSparseElementToEntity<FArcMassPhysicsSimulatingTag>(Entity);
		}
	}

	void ApplyImpulse(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FBodyInstance& Body, FVector Impulse)
	{
		check(IsInGameThread());
		Internal::EnableSimulation(EntityManager, Entity, Body);
		Body.AddImpulse(Impulse, false);
	}

	void AddForce(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FBodyInstance& Body, FVector Force)
	{
		check(IsInGameThread());
		Internal::EnableSimulation(EntityManager, Entity, Body);
		Body.AddForce(Force, true, false);
	}
}
