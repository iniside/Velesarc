// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "CoreMinimal.h"
#include "Sim/ArcBodyUtilityFragments.h"

#include "ArcBodyTraits.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Body", Category = "WorldSim"))
class ARCWORLDSIM_API UArcBodyTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Carrier", meta = (ClampMin = "1"))
	int32 CarryCapacity = 10;

	UPROPERTY(EditAnywhere, Category = "Utility")
	FArcBodyUtilityConfigFragment UtilityConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
	virtual void InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const override;
};
