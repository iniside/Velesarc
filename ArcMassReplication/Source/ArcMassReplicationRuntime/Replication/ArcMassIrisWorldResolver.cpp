// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcMassIrisWorldResolver.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Iris/ReplicationSystem/ObjectReferenceCacheFwd.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"

namespace ArcMassIrisWorldResolver
{
	UWorld* FindWorldForReplicationSystem(const UReplicationSystem* Target)
	{
		if (GEngine == nullptr || Target == nullptr)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType != EWorldType::Game
				&& WorldContext.WorldType != EWorldType::PIE)
			{
				continue;
			}

			UWorld* World = WorldContext.World();
			if (World == nullptr)
			{
				continue;
			}

			const UNetDriver* NetDriver = World->GetNetDriver();
			if (NetDriver == nullptr)
			{
				continue;
			}

			if (NetDriver->GetReplicationSystem() == Target)
			{
				return World;
			}
		}

		UE_LOGF(LogTemp, Warning,"ArcMassReplication: no world matched ReplicationSystem in FindWorldForReplicationSystem");
		return nullptr;
	}

	UWorld* FindWorldForReplicationSystem(const UE::Net::FNetObjectResolveContext& ResolveContext)
	{
		if (ResolveContext.ReplicationSystem == nullptr)
		{
			return nullptr;
		}

		return FindWorldForReplicationSystem(ResolveContext.ReplicationSystem);
	}
}
