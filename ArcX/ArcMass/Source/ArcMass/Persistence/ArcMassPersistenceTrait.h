// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMassPersistenceTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Persistence"))
class ARCMASS_API UArcMassPersistenceTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FArcMassPersistenceConfigFragment PersistenceConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
