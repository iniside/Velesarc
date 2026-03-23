// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcProjectileFragments.h"

#include "ArcHomingProjectileTrait.generated.h"

/** Trait that configures a Mass entity as a homing projectile.
 *  Adds projectile config and homing config for target tracking behavior. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Homing Projectile", Category = "Projectiles"))
class ARCMASS_API UArcHomingProjectileTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FArcProjectileConfigFragment ProjectileConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing")
	FArcProjectileHomingConfigFragment HomingConfig;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
