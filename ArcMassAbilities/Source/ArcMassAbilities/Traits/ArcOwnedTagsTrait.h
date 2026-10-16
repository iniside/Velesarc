// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "ArcOwnedTagsTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Owned Tags", Category = "Abilities"))
class ARCMASSABILITIES_API UArcOwnedTagsTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/**
	 * Tags seeded onto the entity's FArcOwnedTagsFragment at template-build time.
	 * Each tag is added once via FArcTagCountContainer::AddTag; no tag-change events
	 * are broadcast. Multiple traits seeding the same fragment accumulate.
	 */
	UPROPERTY(EditAnywhere, Category = "Tags")
	FGameplayTagContainer DefaultTags;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
};
