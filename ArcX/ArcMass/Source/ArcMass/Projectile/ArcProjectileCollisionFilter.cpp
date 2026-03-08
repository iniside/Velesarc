// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcProjectileCollisionFilter.h"

#include "Chaos/ChaosUserEntity.h"
#include "Chaos/ParticleHandle.h"
#include "Physics/Experimental/ChaosInterfaceWrapper.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"

ECollisionQueryHitType FArcProjectileCollisionFilterCallback::PreFilter(
	const FQueryFilterData& FilterData,
	const Chaos::FPerShapeData& Shape,
	const Chaos::FGeometryParticle& Actor)
{
	if (IgnoredActors && IgnoredActors->Num() > 0)
	{
		FBodyInstance* BodyInstance = nullptr;
		void* UserData = Actor.UserData();
		if (UserData)
		{
			BodyInstance = FChaosUserData::Get<FBodyInstance>(UserData);
			if (!BodyInstance)
			{
				// Check if we appended a custom entity
				FChaosUserEntityAppend* ChaosUserEntityAppend = FChaosUserData::Get<FChaosUserEntityAppend>(UserData);
				if (ChaosUserEntityAppend)
				{
					BodyInstance = FChaosUserData::Get<FBodyInstance>(ChaosUserEntityAppend->ChaosUserData);
				}
			}
		}
		
		if (BodyInstance)
		{
			if (UPrimitiveComponent* Comp = BodyInstance->OwnerComponent.Get())
			{
				AActor* OwnerActor = Comp->GetOwner();
				if (OwnerActor)
				{
					for (const TWeakObjectPtr<AActor>& Ignored : *IgnoredActors)
					{
						if (Ignored.Get() == OwnerActor)
						{
							return ECollisionQueryHitType::None;
						}
					}
				}
			}
		}
	}

	return ECollisionQueryHitType::Block;
}
