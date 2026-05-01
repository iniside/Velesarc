// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"

#include "ArcMassHealthStatsTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Health Stats", Category = "ArcX"))
class ARCMASSDAMAGESYSTEM_API UArcMassHealthStatsTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

private:
	UPROPERTY(EditAnywhere, Category = "Defaults")
	float DefaultHealth = 100.f;

	UPROPERTY(EditAnywhere, Category = "Defaults")
	float DefaultMaxHealth = 100.f;
};
