// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"

#include "ArcMassDamageTrait.generated.h"

class UArcMassDamageResistanceMappingAsset;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Damage", Category = "ArcX"))
class ARCMASSDAMAGESYSTEM_API UArcMassDamageTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

private:
	UPROPERTY(EditAnywhere, Category = "Defaults")
	FInstancedStruct DamageStatsFragment;

	UPROPERTY(EditAnywhere, Category = "Defaults")
	TObjectPtr<UArcMassDamageResistanceMappingAsset> DefaultResistanceMapping;
};
