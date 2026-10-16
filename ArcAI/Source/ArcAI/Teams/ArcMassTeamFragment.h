// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassEntityTraitBase.h"

#include "ArcMassTeamFragment.generated.h"

/** Which team an entity belongs to. Doubles as the key of that team's visibility bitplane in a tiled influence grid.
 *  Never negative: INDEX_NONE is the sentinel for "this entity has no team". */
USTRUCT(BlueprintType)
struct ARCAI_API FArcMassTeamFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team", meta = (ClampMin = "0"))
	int32 TeamId = 0;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Team"))
class ARCAI_API UArcMassTeamTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Team", meta = (ClampMin = "0"))
	int32 TeamId = 0;
};
