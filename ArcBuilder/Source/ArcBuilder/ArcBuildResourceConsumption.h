// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Items/ArcItemSpec.h"
#include "Fragments/ArcMassItemStoreFragment.h"

#include "ArcBuildResourceConsumption.generated.h"

struct FMassEntityManager;
class UArcBuildingDefinition;

/**
 * Base resource consumption strategy for building placement.
 * Check and consume are split so the client preview (check only, read-only) and the
 * server place command (check + consume) share one matching model. Both report the
 * matched items as FArcItemSpec snapshots (copies survive removal; consumed specs
 * feed post-place actions).
 *
 * Base implementation is free placement: always true, no matches.
 * Configured per building definition via instanced struct.
 */
USTRUCT(BlueprintType)
struct ARCBUILDER_API FArcBuildResourceConsumption
{
	GENERATED_BODY()

public:
	/**
	 * Read-only availability check — never mutates any store.
	 * @param OutMatched Optional; filled with spec snapshots of the stacks that would
	 *        be consumed (Amount = stacks taken from that item). Reset on entry.
	 * @return True if every ingredient of the building definition can be satisfied.
	 */
	virtual bool CheckResources(
		const FMassEntityManager& EntityManager,
		FMassEntityHandle InstigatorEntity,
		const UArcBuildingDefinition* BuildingDef,
		TArray<FArcItemSpec>* OutMatched) const;

	/**
	 * Consume the required ingredients (server-side). Re-validates against the live
	 * store before removing anything.
	 * @param OutConsumed Spec snapshots of the stacks actually removed. Reset on entry.
	 * @return True if all ingredients were consumed.
	 */
	virtual bool ConsumeResources(
		FMassEntityManager& EntityManager,
		FMassEntityHandle InstigatorEntity,
		const UArcBuildingDefinition* BuildingDef,
		TArray<FArcItemSpec>& OutConsumed) const;

	virtual ~FArcBuildResourceConsumption() = default;
};

/**
 * Consumes ingredients from an FArcMassItemStoreFragment (or derived store) on the
 * instigator entity. Matching is greedy per ingredient slot and allows one slot to be
 * filled from multiple item stacks; consumption goes through ArcMassItems::RemoveItem
 * so the standard removal rails (events, slot clearing, attachments) fire.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Mass Store Consumption"))
struct ARCBUILDER_API FArcBuildResourceConsumption_MassStore : public FArcBuildResourceConsumption
{
	GENERATED_BODY()

public:
	/** Store fragment type on the instigator entity to draw ingredients from. */
	UPROPERTY(EditAnywhere, Category = "Consumption",
		meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TObjectPtr<UScriptStruct> StoreType = FArcMassItemStoreFragment::StaticStruct();

	virtual bool CheckResources(
		const FMassEntityManager& EntityManager,
		FMassEntityHandle InstigatorEntity,
		const UArcBuildingDefinition* BuildingDef,
		TArray<FArcItemSpec>* OutMatched) const override;

	virtual bool ConsumeResources(
		FMassEntityManager& EntityManager,
		FMassEntityHandle InstigatorEntity,
		const UArcBuildingDefinition* BuildingDef,
		TArray<FArcItemSpec>& OutConsumed) const override;
};
