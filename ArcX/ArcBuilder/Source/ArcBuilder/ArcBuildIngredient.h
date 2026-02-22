// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"

#include "ArcBuildIngredient.generated.h"

struct FArcItemData;

/**
 * Base ingredient requirement for a building.
 * Uses instanced struct pattern for extensibility — new ingredient types
 * can be added without modifying the building definition.
 */
USTRUCT(BlueprintType)
struct ARCBUILDER_API FArcBuildIngredient
{
	GENERATED_BODY()

public:
	/** Display name for the ingredient slot (for UI). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText SlotName;

	/** How many of this ingredient are needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 Amount = 1;

	/** Returns true if the given item satisfies this ingredient requirement. */
	virtual bool DoesItemSatisfy(const FArcItemData* InItemData) const;

	virtual ~FArcBuildIngredient() = default;
};

/**
 * Ingredient matched by a specific item definition.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Specific Item Ingredient"))
struct ARCBUILDER_API FArcBuildIngredient_ItemDef : public FArcBuildIngredient
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemDefinitionId;

	virtual bool DoesItemSatisfy(const FArcItemData* InItemData) const override;
};

/**
 * Ingredient matched by gameplay tags.
 * Matches any item with the required tags that doesn't have the deny tags.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Tag-Based Ingredient"))
struct ARCBUILDER_API FArcBuildIngredient_Tags : public FArcBuildIngredient
{
	GENERATED_BODY()

public:
	/** Tags the item MUST have. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer RequiredTags;

	/** Tags the item must NOT have. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer DenyTags;

	virtual bool DoesItemSatisfy(const FArcItemData* InItemData) const override;
};
