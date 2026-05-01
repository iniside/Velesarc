// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcAbilitiesTrait.generated.h"

class UArcMassAbilitySet;
class UArcEffectDefinition;
class UArcAttributeHandlerConfig;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Abilities System", Category = "Abilities"))
class ARCMASSABILITIES_API UArcAbilitiesTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TObjectPtr<UArcMassAbilitySet>> AbilitySets;

	UPROPERTY(EditAnywhere, Category = "Attributes", meta=(BaseStruct="/Script/MassCore.MassFragment", ExcludeBaseStruct))
	TArray<FInstancedStruct> AttributeFragments;

	UPROPERTY(EditAnywhere, Category = "Handlers")
	TObjectPtr<UArcAttributeHandlerConfig> HandlerConfig = nullptr;

	UPROPERTY(EditAnywhere, Category = "Effects")
	TArray<TObjectPtr<UArcEffectDefinition>> DefaultEffects;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
