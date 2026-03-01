// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcAreaAssignmentFragments.h"

#include "ArcAreaAssignmentTrait.generated.h"

/**
 * Trait that configures an NPC Mass entity for area assignment.
 * Adds the assignment fragment, config shared fragment, and the assignment tag.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Area Assignment"))
class ARCAREA_API UArcAreaAssignmentTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** What roles this NPC archetype is eligible for. */
	UPROPERTY(EditAnywhere, Category = "Area")
	FArcAreaAssignmentConfigFragment AssignmentConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
