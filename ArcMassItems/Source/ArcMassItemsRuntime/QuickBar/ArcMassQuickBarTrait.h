// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "ArcMassQuickBarTrait.generated.h"

class UArcMassQuickBarConfig;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc QuickBar", Category = "Items"))
class ARCMASSITEMSRUNTIME_API UArcMassQuickBarTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "QuickBar")
	TArray<TObjectPtr<UArcMassQuickBarConfig>> Configs;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const override;
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
};
