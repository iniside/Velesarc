// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MassSpawnerTypes.h"
#include "ArcAreaPopulationTypes.generated.h"

class UMassEntityConfigAsset;

/** One entry mapping a role tag to the entity type to spawn for that role. */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaPopulationEntry
{
	GENERATED_BODY()

	/** Role tag that vacancy slots must match (e.g., Role.Blacksmith). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population")
	FGameplayTag RoleTag;

	/** Mass entity config to spawn for this role. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population")
	FMassSpawnedEntityType EntityType;

	/** Maximum number of NPCs with this role the spawner will create. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population", meta = (ClampMin = 0))
	int32 MaxCount = 0;
};

/**
 * Data asset defining the NPC population for an area population spawner.
 * Maps role tags to Mass entity configs with optional count limits.
 * Entry order matters: first matching entry wins for unconstrained slots.
 */
UCLASS(BlueprintType)
class ARCAREA_API UArcAreaPopulationDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population")
	TArray<FArcAreaPopulationEntry> Entries;

	/** Find the entry matching a role tag. Returns nullptr if none found. */
	const FArcAreaPopulationEntry* FindEntryForRole(const FGameplayTag& RoleTag) const;
};
