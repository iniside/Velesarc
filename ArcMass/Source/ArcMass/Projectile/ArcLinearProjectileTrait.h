// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcProjectileTraitBase.h"

#include "ArcLinearProjectileTrait.generated.h"

/** Trait that configures a Mass entity as a linear (straight-line) projectile.
 *  Adds projectile config for speed, lifetime, collision, and damage. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Linear Projectile", Category = "Projectiles"))
class ARCMASS_API UArcLinearProjectileTrait : public UArcProjectileTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
