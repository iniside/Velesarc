// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"

#include "ArcMassDamageTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Damage", Category = "ArcX"))
class ARCMASSDAMAGESYSTEM_API UArcMassDamageTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;

private:
	UPROPERTY(EditAnywhere, Category = "Defaults")
	FInstancedStruct DamageStatsFragment;
};
