// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcSettlementFragments.h"

#include "ArcSettlementTrait.generated.h"

/**
 * Trait that configures a Mass entity as a settlement member (NPC).
 * Adds the member fragment, config shared fragment, and the member tag.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Settlement Member"))
class ARCSETTLEMENT_API UArcSettlementMemberTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** Shared capability configuration for this NPC archetype. */
	UPROPERTY(EditAnywhere, Category = "Settlement")
	FArcSettlementMemberConfigFragment MemberConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
