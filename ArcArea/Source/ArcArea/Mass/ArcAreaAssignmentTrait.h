// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcAreaAssignmentFragments.h"

#include "ArcAreaAssignmentTrait.generated.h"

/**
 * Trait that configures an NPC Mass entity for area assignment.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Area Assignment", Category = "Area"))
class ARCAREA_API UArcAreaAssignmentTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
