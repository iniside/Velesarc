// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcAreaFragments.h"

#include "ArcAreaTrait.generated.h"

class USmartObjectDefinition;
class UStateTree;

/**
 * Trait that configures a Mass entity as an area.
 * Adds the config shared fragment, the area tag, and the SmartObject ownership
 * fragments the observer needs to create the SmartObject on spawn.
 *
 * When StateTree is set, adds MassStateTree shared/instance fragments
 * for StateTree execution. Must use UArcAreaStateTreeSchema.
 *
 * Requires FTransformFragment.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Area", Category = "Area"))
class ARCAREA_API UArcAreaTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** The area definition data asset. */
	UPROPERTY(EditAnywhere, Category = "Area")
	TObjectPtr<UArcAreaDefinition> AreaDefinition;

	/** Mandatory: the area is identified by the handle of the SmartObject spawned from this definition. */
	UPROPERTY(EditAnywhere, Category = "Area")
	TObjectPtr<USmartObjectDefinition> SmartObjectDefinition;

	/** Optional StateTree asset to run on this area. Must use UArcAreaStateTreeSchema. */
	UPROPERTY(EditAnywhere, Category = "StateTree",
		meta = (RequiredAssetDataTags = "Schema=/Script/ArcArea.ArcAreaStateTreeSchema"))
	TObjectPtr<UStateTree> StateTree;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const override;
};
