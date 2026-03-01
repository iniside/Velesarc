// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcKnowledgeTypes.h"
#include "ArcKnowledgeEvent.generated.h"

/** Type of knowledge change that triggered the event. */
UENUM(BlueprintType)
enum class EArcKnowledgeEventType : uint8
{
	Registered,
	Updated,
	Removed,
	AdvertisementPosted,
	AdvertisementClaimed,
	AdvertisementCompleted
};

/**
 * Payload for knowledge change events.
 * Broadcast via AsyncMessage to global listeners and spatial entity endpoints.
 */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgeChangedEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	EArcKnowledgeEventType EventType = EArcKnowledgeEventType::Registered;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FArcKnowledgeHandle Handle;

	/** Tags on the entry at the time of the event. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FGameplayTagContainer Tags;

	/** Location of the knowledge entry. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FVector Location = FVector::ZeroVector;
};
