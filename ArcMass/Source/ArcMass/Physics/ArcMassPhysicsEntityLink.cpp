// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsEntityLink.h"

#include "Chaos/ChaosUserEntity.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsInterfaceTypesCore.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

// ---------------------------------------------------------------------------
// Internal: FChaosUserDefinedEntity subclass carrying FMassEntityHandle
// ---------------------------------------------------------------------------

namespace
{
	const FName ArcMassEntityTypeName(TEXT("ArcMassEntity"));

	class FArcMassPhysicsUserEntity final : public FChaosUserDefinedEntity
	{
	public:
		FMassEntityHandle EntityHandle;

		explicit FArcMassPhysicsUserEntity(FMassEntityHandle InHandle)
			: FChaosUserDefinedEntity(ArcMassEntityTypeName)
			, EntityHandle(InHandle)
		{
		}

		virtual TWeakObjectPtr<UObject> GetOwnerObject() override
		{
			return TWeakObjectPtr<UObject>();
		}
	};

	/** Extract FMassEntityHandle from raw Chaos particle user data pointer. */
	FMassEntityHandle ExtractEntityFromUserData(void* UserData)
	{
		FChaosUserEntityAppend* Append = FChaosUserData::Get<FChaosUserEntityAppend>(UserData);
		if (!Append || !Append->UserDefinedEntity)
		{
			return FMassEntityHandle();
		}

		if (Append->UserDefinedEntity->GetEntityTypeName() != ArcMassEntityTypeName)
		{
			return FMassEntityHandle();
		}

		FArcMassPhysicsUserEntity* MassEntity = static_cast<FArcMassPhysicsUserEntity*>(Append->UserDefinedEntity);
		return MassEntity->EntityHandle;
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

FChaosUserEntityAppend* ArcMassPhysicsEntityLink::Attach(
	FBodyInstance& Body,
	FMassEntityHandle EntityHandle)
{
	check(IsInGameThread());

	FPhysicsActorHandle ActorHandle = Body.GetPhysicsActor();
	if (!ActorHandle)
	{
		return nullptr;
	}

	// Guard against double-attach (mirrors engine check in PhysicsObjectInterface.cpp:831).
	void* ExistingUserData = ActorHandle->GetGameThreadAPI().UserData();
	if (!ensureMsgf(!FChaosUserData::Get<FChaosUserEntityAppend>(ExistingUserData),
		TEXT("ArcMassPhysicsEntityLink::Attach called on a body that already has FChaosUserEntityAppend. Call Detach first.")))
	{
		return nullptr;
	}

	FArcMassPhysicsUserEntity* UserEntity = new FArcMassPhysicsUserEntity(EntityHandle);

	// Follow engine pattern from PhysicsObjectInterface.cpp:836-839.
	// Capture existing user data via reinterpret_cast (same as engine does).
	FChaosUserData* PreviousUserData = reinterpret_cast<FChaosUserData*>(ExistingUserData);
	if (PreviousUserData)
	{
		ensureMsgf(!FChaosUserData::IsGarbage(PreviousUserData),
			TEXT("ArcMassPhysicsEntityLink::Attach: existing user data appears to be garbage."));
	}

	FChaosUserEntityAppend* Append = new FChaosUserEntityAppend();
	Append->ChaosUserData = PreviousUserData;
	Append->UserDefinedEntity = UserEntity;
	ActorHandle->GetGameThreadAPI().SetUserData(Append);

	return Append;
}

void ArcMassPhysicsEntityLink::Detach(FBodyInstance& Body, FChaosUserEntityAppend* Append)
{
	if (!Append)
	{
		return;
	}

	check(IsInGameThread());

	// Restore original user data on the Chaos particle.
	// Follow engine pattern from PhysicsObjectInterface.cpp:851.
	FPhysicsActorHandle ActorHandle = Body.GetPhysicsActor();
	if (ActorHandle)
	{
		void* CurrentUserData = ActorHandle->GetGameThreadAPI().UserData();
		if (CurrentUserData == static_cast<void*>(Append))
		{
			ActorHandle->GetGameThreadAPI().SetUserData(Append->ChaosUserData);
		}
		else
		{
			ensureMsgf(false, TEXT("ArcMassPhysicsEntityLink::Detach: particle user data was replaced after Attach. Restore skipped."));
		}
	}

	delete Append->UserDefinedEntity;
	Append->UserDefinedEntity = nullptr;
	delete Append;
}

FMassEntityHandle ArcMassPhysicsEntityLink::ResolveHit(const FHitResult& HitResult)
{
	check(IsInGameThread());

	// Try component-based path first (ISM instances with owning component).
	UPrimitiveComponent* Component = HitResult.GetComponent();
	if (Component)
	{
		FBodyInstance* Body = Component->GetBodyInstance(NAME_None, /*bGetWelded=*/false, HitResult.Item);
		if (Body)
		{
			FPhysicsActorHandle ActorHandle = Body->GetPhysicsActor();
			if (ActorHandle)
			{
				void* UserData = ActorHandle->GetGameThreadAPI().UserData();
				FMassEntityHandle Result = ExtractEntityFromUserData(UserData);
				if (Result.IsValid())
				{
					return Result;
				}
			}
		}
	}

	// Fallback: try physics object path (componentless bodies).
	return ResolveHitFromPhysicsObject(HitResult);
}

FMassEntityHandle ArcMassPhysicsEntityLink::ResolveHitFromPhysicsObject(const FHitResult& HitResult)
{
	check(IsInGameThread());

	Chaos::FPhysicsObjectHandle PhysicsObject = HitResult.PhysicsObject;
	if (!PhysicsObject)
	{
		return FMassEntityHandle();
	}

	// Get the physics proxy from the PhysicsObject.
	Chaos::FConstPhysicsObjectHandle ConstPhysicsObject = PhysicsObject;
	IPhysicsProxyBase* Proxy = Chaos::FPhysicsObjectInterface::GetProxy({ &ConstPhysicsObject, 1 });
	if (!Proxy || Proxy->GetType() != EPhysicsProxyType::SingleParticleProxy)
	{
		return FMassEntityHandle();
	}

	// SingleParticleProxy is FPhysicsActorHandle. Access the particle's user data.
	Chaos::FSingleParticlePhysicsProxy* SingleProxy = static_cast<Chaos::FSingleParticlePhysicsProxy*>(Proxy);
	void* UserData = SingleProxy->GetGameThreadAPI().UserData();
	return ExtractEntityFromUserData(UserData);
}

FTransform ArcMassPhysicsEntityLink::ResolveHitToBodyTransform(const FHitResult& HitResult)
{
	check(IsInGameThread());

	Chaos::FPhysicsObjectHandle PhysicsObject = HitResult.PhysicsObject;
	if (!PhysicsObject)
	{
		return FTransform::Identity;
	}

	Chaos::FConstPhysicsObjectHandle ConstPhysicsObject = PhysicsObject;
	Chaos::FReadPhysicsObjectInterface_External Interface = FPhysicsObjectExternalInterface::GetRead_AssumesLocked();
	return Interface.GetTransform(ConstPhysicsObject);
}

FBodyInstance* ArcMassPhysicsEntityLink::ResolveHitToBody(const FHitResult& HitResult)
{
	check(IsInGameThread());

	Chaos::FPhysicsObjectHandle PhysicsObject = HitResult.PhysicsObject;
	if (!PhysicsObject)
	{
		return nullptr;
	}

	Chaos::FConstPhysicsObjectHandle ConstPhysicsObject = PhysicsObject;
	IPhysicsProxyBase* Proxy = Chaos::FPhysicsObjectInterface::GetProxy({ &ConstPhysicsObject, 1 });
	if (!Proxy || Proxy->GetType() != EPhysicsProxyType::SingleParticleProxy)
	{
		return nullptr;
	}

	Chaos::FSingleParticlePhysicsProxy* SingleProxy = static_cast<Chaos::FSingleParticlePhysicsProxy*>(Proxy);
	void* UserData = SingleProxy->GetGameThreadAPI().UserData();

	FBodyInstance* BodyInstance = FChaosUserData::Get<FBodyInstance>(UserData);
	if (!BodyInstance)
	{
		FChaosUserEntityAppend* Append = FChaosUserData::Get<FChaosUserEntityAppend>(UserData);
		if (Append)
		{
			BodyInstance = FChaosUserData::Get<FBodyInstance>(Append->ChaosUserData);
		}
	}
	return BodyInstance;
}
