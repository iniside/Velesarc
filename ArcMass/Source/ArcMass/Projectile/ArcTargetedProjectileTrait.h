// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcProjectileTraitBase.h"

#include "ArcTargetedProjectileTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Targeted Projectile", Category = "Projectiles"))
class ARCMASS_API UArcTargetedProjectileTrait : public UArcProjectileTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Targeted")
	FArcTargetedProjectileConfigFragment TargetedConfig;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
