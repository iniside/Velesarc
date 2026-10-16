// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcSettlementCenter.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcBuildingTraits.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Building", Category = "WorldSim"))
class ARCWORLDSIM_API UArcBuildingTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "WorldSim")
	FText Label;

	UPROPERTY(EditAnywhere, Category = "Functions",
		meta = (BaseStruct = "/Script/ArcWorldSim.ArcBuildingFunctionConfig", ExcludeBaseStruct))
	TArray<FInstancedStruct> FunctionConfigs;

	bool ValidateFunctionConfigs(FString& OutError) const;
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World,
		FAdditionalTraitRequirements& OutTraitRequirements) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Gather Area", Category = "WorldSim"))
class ARCWORLDSIM_API UArcGatherAreaTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Gathering", meta = (ShowOnlyInnerProperties))
	FArcGatherAreaConfigFragment Config;

	UPROPERTY(EditAnywhere, Category = "WorldSim", meta = (ShowOnlyInnerProperties))
	FArcWorldSimLabelFragment Label;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Settlement Center", Category = "WorldSim"))
class ARCWORLDSIM_API UArcSettlementCenterTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settlement", meta = (ShowOnlyInnerProperties))
	FArcSettlementCenterFragment Center;

	UPROPERTY(EditAnywhere, Category = "WorldSim", meta = (ShowOnlyInnerProperties))
	FArcWorldSimLabelFragment Label;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
