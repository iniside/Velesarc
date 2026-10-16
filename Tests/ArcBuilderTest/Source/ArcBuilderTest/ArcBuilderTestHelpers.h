// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "UObject/PrimaryAssetId.h"
#include "Attributes/ArcAttribute.h"

#include "ArcBuildRequirement.h"

#include "ArcBuilderTestHelpers.generated.h"

struct FMassEntityManager;
class FArcItemTestRecordFiles;
class UArcBuildingDefinition;
struct FArcItemSpec;

/**
 * Minimal test-only attribute fragment (no production ArcMassDamageSystem dependency —
 * that module doesn't expose PublicIncludePaths for cross-plugin header consumption).
 * Mirrors FArcAttributeSet_Health's shape closely enough to exercise
 * FArcBuildPostPlaceAction_IngredientStats's fragment-write path.
 */
USTRUCT(meta = (Hidden))
struct FArcTestAttributeSet_Vitality : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Health;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcTestAttributeSet_Vitality, Health)
};

template <>
struct TMassFragmentTraits<FArcTestAttributeSet_Vitality>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Placement requirement fixture whose CanPlace() result is fully controlled by the test —
 * used to exercise FArcPlaceBuildingCommand's requirement-refusal gate without needing a
 * real spatial-hash/station setup.
 */
USTRUCT(meta = (Hidden))
struct FArcBuildRequirement_TestControlled : public FArcBuildRequirement
{
	GENERATED_BODY()

	bool bResult = true;

	// Number of times CanPlace was invoked — lets a test assert the gate was actually reached.
	mutable int32 NumCanPlaceCalls = 0;

	virtual bool CanPlace(
		const FTransform& InTransform,
		const FArcBuildRequirementContext& InContext,
		const UArcBuildingDefinition* InBuildDef) const override
	{
		++NumCanPlaceCalls;
		return bResult;
	}
};

namespace ArcBuilderTest::TestHelpers
{
	/** Item definition record carrying FArcItemFragment_Tags::AssetTags = { Tag }, written and indexed through Records. */
	FPrimaryAssetId MakeTaggedItemDef(FArcItemTestRecordFiles& Records, const FGameplayTag& InTag);

	/** Minimal FArcItemSpec for a definition id with a fresh item id and the given stack count. */
	FArcItemSpec MakeSpec(const FPrimaryAssetId& DefId, int32 Amount);

	/** Entity carrying a regular transform fragment + an empty FArcMassItemStoreFragment. */
	FMassEntityHandle CreateStoreEntity(FMassEntityManager& EntityManager);

	/** Entity carrying only a transform fragment — stand-in instigator with no item store. */
	FMassEntityHandle CreateBareEntity(FMassEntityManager& EntityManager);

	/** Transient UArcBuildingDefinition (BuildingId auto-generated in PostInitProperties). */
	UArcBuildingDefinition* MakeTransientBuildingDef(const FName& InName);

	/**
	 * Registers Def with UAssetManager via AddDynamicAsset so FArcPlaceBuildingCommand's
	 * UAssetManager::GetPrimaryAssetObject / GetPrimaryAssetPath().TryLoad() resolution
	 * finds it. Sets Def->BuildingType = InType before computing the primary asset id
	 * (BuildingId is already valid from construction). Requires >=1 bundle entry or
	 * AddDynamicAsset no-ops. Returns the registered id (invalid on failure).
	 */
	FPrimaryAssetId RegisterBuildingDefDynamic(UArcBuildingDefinition* Def, const FPrimaryAssetType& InType);

	/** AFTER_EACH cleanup counterpart — empty path + empty bundle data is the removal path. */
	void UnregisterDynamicAsset(const FPrimaryAssetId& Id);
}
