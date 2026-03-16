// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcProjectileFragments.h"

#include "ArcLinearProjectileTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class ARCMASS_API UArcLinearProjectileTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FArcProjectileConfigFragment ProjectileConfig;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
