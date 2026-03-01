// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsyncMessageId.h"
#include "ArcKnowledgeEvent.h"
#include "ArcKnowledgeEventBroadcaster.generated.h"

class FAsyncMessageSystemBase;
class UArcMassSpatialHashSubsystem;
class UAsyncMessageWorldSubsystem;
struct FMassEntityManager;

/** Configuration for knowledge event broadcasting. */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgeEventBroadcastConfig
{
	GENERATED_BODY()

	/** AsyncMessage ID for global knowledge events (all listeners). */
	UPROPERTY(EditAnywhere, Category = "Events")
	FAsyncMessageId GlobalMessageId;

	/** AsyncMessage ID for spatial knowledge events (per-entity endpoint). */
	UPROPERTY(EditAnywhere, Category = "Events")
	FAsyncMessageId SpatialMessageId;

	/** Default spatial broadcast radius when entry doesn't override. */
	UPROPERTY(EditAnywhere, Category = "Events", meta = (ClampMin = "0.0"))
	float DefaultSpatialRadius = 5000.0f;
};

/**
 * Broadcasts knowledge change events through two channels:
 * - Global: AsyncMessage on default endpoint (all listeners receive)
 * - Spatial: AsyncMessage on per-entity endpoints (only nearby entities receive)
 *
 * Owned by UArcKnowledgeSubsystem. Not a UObject — plain C++ for minimal overhead.
 */
class ARCKNOWLEDGE_API FArcKnowledgeEventBroadcaster
{
public:
	/** Initialize with world and configuration. Call from subsystem Initialize(). */
	void Initialize(UAsyncMessageWorldSubsystem* AsyncMessage, const FArcKnowledgeEventBroadcastConfig& InConfig);

	/** Clean up. Call from subsystem Deinitialize(). */
	void Shutdown();

	/**
	 * Broadcast a knowledge event.
	 * @param Event The event payload.
	 * @param SpatialRadius Per-entry radius override. 0 = use config default.
	 */
	void BroadcastEvent(const FArcKnowledgeChangedEvent& Event, float SpatialRadius = 0.0f);

	const FArcKnowledgeEventBroadcastConfig& GetConfig() const { return Config; }

private:
	void BroadcastGlobal(const FArcKnowledgeChangedEvent& Event);
	void BroadcastSpatial(const FArcKnowledgeChangedEvent& Event, float Radius);

	FArcKnowledgeEventBroadcastConfig Config;
	TWeakObjectPtr<UWorld> World;
	TSharedPtr<FAsyncMessageSystemBase> MessageSystem;
	UArcMassSpatialHashSubsystem* SpatialHashSubsystem = nullptr;
	FMassEntityManager* EntityManager = nullptr;
};
