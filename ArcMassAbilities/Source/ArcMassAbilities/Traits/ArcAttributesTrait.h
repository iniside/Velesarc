// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcAttributesTrait.generated.h"

class UArcEffectDefinition;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Attributes", Category = "Abilities"))
class ARCMASSABILITIES_API UArcAttributesTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Attributes", meta=(BaseStruct="/Script/MassCore.MassFragment", ExcludeBaseStruct))
	TArray<FInstancedStruct> AttributeFragments;

	UPROPERTY(EditAnywhere, Category = "Effects")
	TArray<TObjectPtr<UArcEffectDefinition>> DefaultEffects;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
