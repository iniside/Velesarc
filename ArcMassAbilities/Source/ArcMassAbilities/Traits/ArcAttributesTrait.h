// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcAttributesTrait.generated.h"

class UArcEffectDefinition;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Attributes", Category = "Abilities"))
class ARCMASSABILITIES_API UArcAttributesTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Attributes", meta=(BaseStruct="/Script/ArcMassAbilities.ArcMassAttributesBase", ExcludeBaseStruct))
	TArray<FInstancedStruct> AttributeFragments;

	UPROPERTY(EditAnywhere, Category = "Effects")
	TArray<TObjectPtr<UArcEffectDefinition>> DefaultEffects;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
};
