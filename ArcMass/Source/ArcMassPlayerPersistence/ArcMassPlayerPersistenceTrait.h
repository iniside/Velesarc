// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMassPlayerPersistenceTrait.generated.h"

/**
 * Trait that marks a player's Mass entity for player-scoped persistence.
 * Adds FArcMassPlayerPersistenceTag and optionally a
 * FArcMassPersistenceConfigFragment for fragment filtering.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories,
	meta = (DisplayName = "Player Persistence"))
class ARCMASSPLAYERPERSISTENCE_API UArcMassPlayerPersistenceTrait
	: public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Persistence")
	FArcMassPersistenceConfigFragment PersistenceConfig;

	virtual void BuildTemplate(
		FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override;
};
