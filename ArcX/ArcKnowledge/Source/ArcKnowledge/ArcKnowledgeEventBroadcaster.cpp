// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeEventBroadcaster.h"
#include "AsyncMessageSystemBase.h"
#include "AsyncMessageWorldSubsystem.h"
#include "AsyncGameplayMessageSystem.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
#include "ArcMass/ArcMassAsyncMessageEndpointFragment.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Engine/World.h"
#include "StructUtils/StructView.h"

void FArcKnowledgeEventBroadcaster::Initialize(UAsyncMessageWorldSubsystem* AsyncMessage, const FArcKnowledgeEventBroadcastConfig& InConfig)
{

	Config = InConfig;

	if (!AsyncMessage)
	{
		return;
	}

	World = AsyncMessage->GetWorld();

	MessageSystem = AsyncMessage->GetSharedMessageSystem<FAsyncGameplayMessageSystem>();
	SpatialHashSubsystem = World->GetSubsystem<UArcMassSpatialHashSubsystem>();

	if (UMassEntitySubsystem* MassEntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
	{
		EntityManager = &MassEntitySubsystem->GetMutableEntityManager();
	}
}

void FArcKnowledgeEventBroadcaster::Shutdown()
{
	MessageSystem.Reset();
	SpatialHashSubsystem = nullptr;
	EntityManager = nullptr;
	World.Reset();
}

void FArcKnowledgeEventBroadcaster::BroadcastEvent(const FArcKnowledgeChangedEvent& Event, float SpatialRadius)
{
	BroadcastGlobal(Event);

	if (!Event.Location.IsZero())
	{
		const float EffectiveRadius = SpatialRadius > 0.0f ? SpatialRadius : Config.DefaultSpatialRadius;
		if (EffectiveRadius > 0.0f)
		{
			BroadcastSpatial(Event, EffectiveRadius);
		}
	}
}

void FArcKnowledgeEventBroadcaster::BroadcastGlobal(const FArcKnowledgeChangedEvent& Event)
{
	if (!MessageSystem.IsValid() || !Config.GlobalMessageId.IsValid())
	{
		return;
	}

	MessageSystem->QueueMessageForBroadcast(
		Config.GlobalMessageId,
		FConstStructView::Make(Event));
}

void FArcKnowledgeEventBroadcaster::BroadcastSpatial(const FArcKnowledgeChangedEvent& Event, float Radius)
{
	if (!MessageSystem.IsValid() || !Config.SpatialMessageId.IsValid())
	{
		return;
	}

	if (!SpatialHashSubsystem || !EntityManager)
	{
		return;
	}

	TArray<FArcMassEntityInfo> NearbyEntities;
	SpatialHashSubsystem->QuerySphere(Event.Location, Radius, NearbyEntities);

	for (const FArcMassEntityInfo& EntityInfo : NearbyEntities)
	{
		if (!EntityManager->IsEntityValid(EntityInfo.Entity))
		{
			continue;
		}

		const FArcMassAsyncMessageEndpointFragment* EndpointFragment =
			EntityManager->GetFragmentDataPtr<FArcMassAsyncMessageEndpointFragment>(EntityInfo.Entity);

		if (!EndpointFragment || !EndpointFragment->Endpoint.IsValid())
		{
			continue;
		}

		MessageSystem->QueueMessageForBroadcast(
			Config.SpatialMessageId,
			FConstStructView::Make(Event),
			EndpointFragment->Endpoint);
	}
}
