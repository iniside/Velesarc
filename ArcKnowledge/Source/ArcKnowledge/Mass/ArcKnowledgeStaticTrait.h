// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcKnowledgeStaticFragment.h"

#include "ArcKnowledgeStaticTrait.generated.h"

/**
 * Trait that configures a Mass entity as a static knowledge source.
 * Adds the static config shared fragment and the static tag.
 * The entity's position is inserted into the knowledge R-tree on spawn.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Knowledge Static", Category = "Knowledge"))
class ARCKNOWLEDGE_API UArcKnowledgeStaticTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** Shared static knowledge configuration for this archetype. */
	UPROPERTY(EditAnywhere, Category = "Knowledge")
	FArcKnowledgeStaticConfigFragment StaticConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
