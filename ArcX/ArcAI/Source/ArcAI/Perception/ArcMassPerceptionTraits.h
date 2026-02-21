// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassSightPerception.h"
#include "ArcMassHearingPerception.h"

#include "ArcMassPerceptionTraits.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Perception Combined Perceiver"))
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

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Perception Combined Perceivable"))
class ARCAI_API UArcPerceptionCombinedPerceivableTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
