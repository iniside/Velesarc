// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcProjectileUtils.h"

#include "ArcProjectileFragments.h"

#include "Chaos/ChaosUserEntity.h"
#include "Chaos/ParticleHandle.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"

void UE::ArcMass::Projectile::ResolveHitActorComponent(
	const Chaos::FGeometryParticle* Particle,
	AActor*& OutActor,
	UPrimitiveComponent*& OutComponent)
{
	OutActor = nullptr;
	OutComponent = nullptr;

	if (!Particle)
	{
		return;
	}

	FBodyInstance* BodyInstance = nullptr;
	void* UserData = Particle->UserData();
	if (UserData)
	{
		BodyInstance = FChaosUserData::Get<FBodyInstance>(UserData);
		if (!BodyInstance)
		{
			FChaosUserEntityAppend* ChaosUserEntityAppend = FChaosUserData::Get<FChaosUserEntityAppend>(UserData);
			if (ChaosUserEntityAppend)
			{
				BodyInstance = FChaosUserData::Get<FBodyInstance>(ChaosUserEntityAppend->ChaosUserData);
			}
		}
	}

	if (BodyInstance)
	{
		OutComponent = BodyInstance->OwnerComponent.Get();
		if (OutComponent)
		{
			OutActor = OutComponent->GetOwner();
		}
	}
}

FHitResult UE::ArcMass::Projectile::BuildRaycastHitResult(
	const FVector& HitPos,
	const FVector& HitNormal,
	float Distance,
	int32 FaceIndex,
	AActor* HitActor,
	UPrimitiveComponent* HitComponent,
	const FVector& TraceStart,
	const FVector& TraceEnd)
{
	FHitResult HitResult;
	HitResult.ImpactPoint = HitPos;
	HitResult.ImpactNormal = HitNormal;
	HitResult.Location = HitPos;
	HitResult.Normal = HitNormal;
	HitResult.Distance = Distance;
	HitResult.TraceStart = TraceStart;
	HitResult.TraceEnd = TraceEnd;
	HitResult.FaceIndex = FaceIndex;

	if (HitActor)
	{
		HitResult.HitObjectHandle = FActorInstanceHandle(HitActor);
	}
	if (HitComponent)
	{
		HitResult.Component = HitComponent;
	}

	return HitResult;
}

FHitResult UE::ArcMass::Projectile::BuildOverlapHitResult(
	const FVector& OverlapPos,
	AActor* HitActor,
	UPrimitiveComponent* HitComponent,
	const FVector& TraceStart)
{
	FHitResult HitResult;
	HitResult.ImpactPoint = OverlapPos;
	HitResult.Location = OverlapPos;
	HitResult.TraceStart = TraceStart;
	HitResult.TraceEnd = OverlapPos;

	if (HitActor)
	{
		HitResult.HitObjectHandle = FActorInstanceHandle(HitActor);
	}
	if (HitComponent)
	{
		HitResult.Component = HitComponent;
	}

	return HitResult;
}

void UE::ArcMass::Projectile::DispatchHitNotifications(const TArray<FPendingHitNotification>& PendingHits)
{
	for (const FPendingHitNotification& Pending : PendingHits)
	{
		FArcCollisionHitContext HitContext;
		HitContext.HitResult = Pending.HitResult;
		HitContext.Instigator = Pending.Instigator;
		HitContext.Velocity = Pending.Velocity;
		HitContext.bExpired = Pending.bExpired;

		// Notify visualization actor
		if (AActor* Actor = Pending.VisualizationActor)
		{
			if (Actor->Implements<UArcProjectileActorInterface>())
			{
				if (Pending.bExpired)
				{
					IArcProjectileActorInterface::Execute_OnProjectileExpired(Actor, HitContext);
				}
				else
				{
					IArcProjectileActorInterface::Execute_OnProjectileCollision(Actor, HitContext);
				}
			}
		}

		// Notify instigator
		UObject* Instigator = Pending.Instigator.Get();
		if (Instigator && Instigator->Implements<UArcCollisionHitHandler>())
		{
			IArcCollisionHitHandler::Execute_OnCollisionHit(Instigator, HitContext);
		}
	}
}
