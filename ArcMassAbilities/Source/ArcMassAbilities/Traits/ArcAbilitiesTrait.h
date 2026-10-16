// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "ArcAbilitiesTrait.generated.h"

class UArcMassAbilitySet;
class UArcAttributeHandlerConfig;
class UArcInitialAttributesPreset;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Abilities System", Category = "Abilities"))
class ARCMASSABILITIES_API UArcAbilitiesTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TObjectPtr<UArcMassAbilitySet>> AbilitySets;

	/**
	 * Shared preset asset supplying the authored BaseValues for the entity's attribute
	 * fragments. The initial-attributes observer copies BaseValues from this preset onto the
	 * entity's attribute fragments at spawn, then fires PostChange so derived stats cascade.
	 * Because the preset is a standalone UDataAsset it can be shared across any number of
	 * MassEntityConfig / UArcAbilitiesTrait instances.
	 */
	UPROPERTY(EditAnywhere, Category = "Attributes")
	TObjectPtr<UArcInitialAttributesPreset> InitialAttributesPreset = nullptr;

	UPROPERTY(EditAnywhere, Category = "Handlers")
	TObjectPtr<UArcAttributeHandlerConfig> HandlerConfig = nullptr;

	/**
	 * Tags seeded onto the entity's FArcOwnedTagsFragment at template-build time.
	 * Each tag is added once via FArcTagCountContainer::AddTag; no tag-change events
	 * are broadcast. Multiple traits on the same config accumulate.
	 */
	UPROPERTY(EditAnywhere, Category = "Tags")
	FGameplayTagContainer DefaultTags;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
	virtual void InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const override;
};
