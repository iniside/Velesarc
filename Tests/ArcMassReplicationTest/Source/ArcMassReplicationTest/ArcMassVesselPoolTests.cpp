// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Replication/ArcMassEntityVessel.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"
#include "UObject/UObjectGlobals.h"

/**
 * Friend accessor that exposes the subsystem's private pool ops to tests.
 * Production callers must go through RegisterEntity/UnregisterEntity.
 */
struct FArcMassEntityReplicationSubsystemTestAccess
{
	static UArcMassEntityVessel* AcquireVessel(UArcMassEntityReplicationSubsystem& Subsystem)
	{
		return Subsystem.AcquireVessel();
	}

	static void ReleaseVessel(UArcMassEntityReplicationSubsystem& Subsystem, UArcMassEntityVessel* Vessel)
	{
		Subsystem.ReleaseVessel(Vessel);
	}
};

TEST_CLASS(ArcMassVesselPoolTest, "Arc.MassReplication.VesselPool")
{
	UWorld* TestWorld = nullptr;
	UArcMassEntityReplicationSubsystem* Subsystem = nullptr;

	BEFORE_EACH()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/ false);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(TestWorld);
		Subsystem = TestWorld->GetSubsystem<UArcMassEntityReplicationSubsystem>();
		ASSERT_THAT(IsNotNull(Subsystem));
	}

	AFTER_EACH()
	{
		if (TestWorld != nullptr)
		{
			GEngine->DestroyWorldContext(TestWorld);
			TestWorld->DestroyWorld(false);
		}
		TestWorld = nullptr;
		Subsystem = nullptr;
	}

	TEST_METHOD(AcquireAndRelease_RoundTripsTheSameVessel)
	{
		UArcMassEntityVessel* First = FArcMassEntityReplicationSubsystemTestAccess::AcquireVessel(*Subsystem);
		ASSERT_THAT(IsNotNull(First));
		FArcMassEntityReplicationSubsystemTestAccess::ReleaseVessel(*Subsystem, First);
		UArcMassEntityVessel* Second = FArcMassEntityReplicationSubsystemTestAccess::AcquireVessel(*Subsystem);
		ASSERT_THAT(AreEqual(First, Second));
	}

	TEST_METHOD(GarbageCollect_DoesNotCollectPooledVessels)
	{
		UArcMassEntityVessel* PooledVessel = FArcMassEntityReplicationSubsystemTestAccess::AcquireVessel(*Subsystem);
		FArcMassEntityReplicationSubsystemTestAccess::ReleaseVessel(*Subsystem, PooledVessel);
		CollectGarbage(RF_NoFlags);
		ASSERT_THAT(AreEqual(true, IsValid(PooledVessel)));
	}
};
