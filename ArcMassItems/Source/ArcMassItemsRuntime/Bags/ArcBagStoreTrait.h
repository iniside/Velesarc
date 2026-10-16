// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "ArcBagStoreTrait.generated.h"

class UArcBagDefinition;

/**
 * Entity trait that seeds a FArcMassBagStoreFragment on an entity and populates
 * it with initial bag instances at spawn time.
 *
 * BuildSparseFragments seeds one shared template fragment (TargetStoreType only).
 * InitializeEntity runs per-entity so each bag gets a unique FArcBagId.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Bag Store", Category = "Items"))
class ARCMASSITEMSRUNTIME_API UArcMassBagStoreTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/**
	 * The item store struct type that the bags in this overlay index into.
	 * When null, defaults to FArcMassItemStoreFragment.
	 */
	UPROPERTY(EditAnywhere, Category = "Items",
		meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TObjectPtr<UScriptStruct> TargetStoreType;

	/** Bag definitions to create on each spawned entity. */
	UPROPERTY(EditAnywhere, Category = "Items")
	TArray<TObjectPtr<UArcBagDefinition>> InitialBags;

	/**
	 * Seeds one FArcMassBagStoreFragment template with TargetStoreType set.
	 * Bag instances are NOT populated here — they are created per-entity in
	 * InitializeEntity so each bag gets a unique FArcBagId::Generate() result.
	 */
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;

	/**
	 * Creates per-entity FArcBagInstance entries from InitialBags.
	 * Runs after BuildSparseFragments has already placed the fragment on the entity.
	 */
	virtual void InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const override;
};
