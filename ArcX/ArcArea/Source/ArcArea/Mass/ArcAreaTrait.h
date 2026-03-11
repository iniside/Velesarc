// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcAreaFragments.h"

#include "ArcAreaTrait.generated.h"

class USmartObjectDefinition;
class UStateTree;

/**
 * Trait that configures a Mass entity as an area.
 * Adds the area fragment, config shared fragment, and the area tag.
 *
 * When SmartObjectDefinition is set, also adds FArcSmartObjectOwnerFragment
 * and FArcSmartObjectDefinitionSharedFragment so the observer can create
 * the SmartObject on spawn.
 *
 * When StateTree is set, adds MassStateTree shared/instance fragments
 * for StateTree execution. Must use UArcAreaStateTreeSchema.
 *
 * Requires FTransformFragment.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Area"))
class ARCAREA_API UArcAreaTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** The area definition data asset. */
	UPROPERTY(EditAnywhere, Category = "Area")
	TObjectPtr<UArcAreaDefinition> AreaDefinition;

	/** Optional SmartObject definition. When set, the area observer creates a SmartObject
	  * at the entity's location on spawn and stores the handle in FArcSmartObjectOwnerFragment. */
	UPROPERTY(EditAnywhere, Category = "Area")
	TObjectPtr<USmartObjectDefinition> SmartObjectDefinition;

	/** Optional StateTree asset to run on this area. Must use UArcAreaStateTreeSchema. */
	UPROPERTY(EditAnywhere, Category = "StateTree",
		meta = (RequiredAssetDataTags = "Schema=/Script/ArcArea.ArcAreaStateTreeSchema"))
	TObjectPtr<UStateTree> StateTree;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const override;
};
