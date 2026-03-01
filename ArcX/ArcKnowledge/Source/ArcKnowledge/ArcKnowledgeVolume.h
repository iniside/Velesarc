// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "ArcKnowledgeTypes.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeVolume.generated.h"

class USphereComponent;
class UArcKnowledgeEntryDefinition;

/**
 * Level-placed actor that marks a knowledge area.
 * Registers a knowledge entry (with configurable tags and payload) on BeginPlay,
 * and optionally registers additional initial knowledge entries.
 * Can reference a UArcKnowledgeEntryDefinition for reusable presets.
 * Cleans up all registered knowledge on EndPlay.
 */
UCLASS()
class ARCKNOWLEDGE_API AArcKnowledgeVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcKnowledgeVolume();

	/** Optional data asset defining this knowledge area's entries.
	  * When set, its Tags/Payload/InitialKnowledge are used as defaults.
	  * Per-instance properties below override the definition when non-empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	TObjectPtr<UArcKnowledgeEntryDefinition> Definition;

	/** Tags to put on the knowledge entry registered by this volume.
	  * Overrides Definition.Tags when non-empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	FGameplayTagContainer KnowledgeTags;

	/** Optional payload data for the knowledge entry.
	  * Overrides Definition.Payload when set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge", meta = (BaseStruct = "/Script/ArcKnowledge.ArcKnowledgePayload", ExcludeBaseStruct))
	FInstancedStruct Payload;

	/** Additional knowledge entries to register alongside this volume's own entry.
	  * Combined with Definition.InitialKnowledge (definition entries registered first). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	TArray<FArcKnowledgeEntry> InitialKnowledge;

	/** Spatial broadcast radius for events on this area's knowledge. 0 = use system default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge", meta = (ClampMin = "0.0"))
	float SpatialBroadcastRadius = 0.0f;

	/** Visual/spatial representation of the knowledge area bounds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Knowledge")
	TObjectPtr<USphereComponent> SphereComponent;

	/** The runtime handle for the primary knowledge entry. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Knowledge")
	FArcKnowledgeHandle KnowledgeHandle;

	/** Runtime handles for initial knowledge entries. */
	TArray<FArcKnowledgeHandle> InitialKnowledgeHandles;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
