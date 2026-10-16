// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassSightPerception.h"
#include "ArcMassHearingPerception.h"

#include "ArcMassPerceptionTraits.generated.h"

/** Trait that adds both sight and hearing perception to a Mass entity.
 *  Convenience trait combining sight cone and hearing detection in a single config. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Perception Combined Perceiver", Category = "Perception"))
class ARCAI_API UArcPerceptionCombinedPerceiverTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FArcPerceptionSightSenseConfigFragment SightConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FArcPerceptionHearingSenseConfigFragment HearingConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that marks a Mass entity as perceivable by both sight and hearing.
 *  Adds both perceivable tags so the entity can be detected visually and audibly. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Perception Combined Perceivable", Category = "Perception"))
class ARCAI_API UArcPerceptionCombinedPerceivableTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
