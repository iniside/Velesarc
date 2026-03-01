// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcKnowledgeFragments.h"

#include "ArcKnowledgeTrait.generated.h"

/**
 * Trait that configures a Mass entity as a knowledge member (NPC).
 * Adds the member fragment, config shared fragment, and the member tag.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Knowledge Member"))
class ARCKNOWLEDGE_API UArcKnowledgeMemberTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** Shared capability configuration for this NPC archetype. */
	UPROPERTY(EditAnywhere, Category = "Knowledge")
	FArcKnowledgeMemberConfigFragment MemberConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
