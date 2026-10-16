// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "ArcMassItemStoreTrait.generated.h"

// Seeds one sparse item-store fragment per entry in StoreTypes, plus a singleton
// FArcMassItemEventFragment. To run multiple stores on one entity (e.g. a
// high-churn inventory store and a static equipment store), derive
// FArcMassItemStoreFragment and list each derived type here.
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Item Store", Category = "Items"))
class ARCMASSITEMSRUNTIME_API UArcMassItemStoreTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	// Sparse store fragments to seed. Empty == {FArcMassItemStoreFragment}.
	UPROPERTY(EditAnywhere, Category = "Items",
		meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TArray<TObjectPtr<UScriptStruct>> StoreTypes;

	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
};
