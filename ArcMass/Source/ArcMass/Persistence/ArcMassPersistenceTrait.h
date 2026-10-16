// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMassPersistenceTrait.generated.h"

/** Trait that opts a Mass entity into the world persistence system.
 *  Adds persistence config for save/load of entity state across sessions. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Persistence", Category = "Persistence"))
class ARCMASS_API UArcMassPersistenceTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FArcMassPersistenceConfigFragment PersistenceConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
