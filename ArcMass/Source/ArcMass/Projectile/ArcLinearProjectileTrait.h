// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcProjectileFragments.h"

#include "ArcLinearProjectileTrait.generated.h"

/** Trait that configures a Mass entity as a linear (straight-line) projectile.
 *  Adds projectile config for speed, lifetime, collision, and damage. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Linear Projectile", Category = "Projectiles"))
class ARCMASS_API UArcLinearProjectileTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FArcProjectileConfigFragment ProjectileConfig;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
