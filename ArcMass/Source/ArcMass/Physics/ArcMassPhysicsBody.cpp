// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBody.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Async/Async.h"
#include "Physics/PhysicsInterfaceCore.h"

void FArcMassPhysicsBodyFragment::TerminateBody()
{
	if (Body)
	{
		FBodyInstance::FAsyncTermBodyPayload Payload = Body->StartAsyncTermBody_GameThread();
		delete Body;
		Body = nullptr;

		if (FPhysicsInterface::IsValid(Payload.GetPhysicsActor()))
		{
			AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
				[Payload = MoveTemp(Payload)]() mutable
				{
					FBodyInstance::AsyncTermBody(Payload);
				});
		}
	}
}

void UE::ArcMass::Physics::AsyncTermBodies(TArray<FBodyInstance::FAsyncTermBodyPayload>&& Payloads)
{
	if (Payloads.Num() == 0)
	{
		return;
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[Payloads = MoveTemp(Payloads)]() mutable
		{
			for (FBodyInstance::FAsyncTermBodyPayload& Payload : Payloads)
			{
				FBodyInstance::AsyncTermBody(Payload);
			}
		});
}
