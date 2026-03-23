// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcProjectileFragments.h"

#include "ArcBouncingProjectileTrait.generated.h"

/** Trait that configures a Mass entity as a bouncing projectile.
 *  Adds projectile config and bounce config for ricochet behavior on impact. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Bouncing Projectile", Category = "Projectiles"))
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
