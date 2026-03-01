// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcAreaStateTreeTrait.generated.h"

class UStateTree;

/**
 * Trait that adds StateTree execution to an Area entity.
 * Filters to UArcAreaStateTreeSchema so only Area-appropriate trees can be assigned.
 * Requires UArcAreaTrait on the same entity.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Area StateTree"))
class ARCAREA_API UArcAreaStateTreeTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const override;

protected:
	/** StateTree asset to run on this area. Must use UArcAreaStateTreeSchema. */
	UPROPERTY(EditAnywhere, Category = "StateTree",
		meta = (RequiredAssetDataTags = "Schema=/Script/ArcArea.ArcAreaStateTreeSchema"))
	TObjectPtr<UStateTree> StateTree;
};
