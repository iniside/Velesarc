// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcAreaFragments.h"

#include "ArcAreaTrait.generated.h"

class USmartObjectDefinition;

/**
 * Trait that configures a Mass entity as an area.
 * Adds the area fragment, config shared fragment, and the area tag.
 *
 * When SmartObjectDefinition is set, also adds FArcSmartObjectOwnerFragment
 * and FArcSmartObjectDefinitionSharedFragment so the observer can create
 * the SmartObject on spawn.
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

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
