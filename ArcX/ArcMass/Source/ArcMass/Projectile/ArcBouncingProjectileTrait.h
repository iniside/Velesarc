// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcProjectileFragments.h"

#include "ArcBouncingProjectileTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class ARCMASS_API UArcBouncingProjectileTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FArcProjectileConfigFragment ProjectileConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Bounce")
	FArcProjectileBounceConfigFragment BounceConfig;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
