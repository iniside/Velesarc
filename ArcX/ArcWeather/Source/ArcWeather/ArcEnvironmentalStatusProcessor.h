// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcEnvironmentalStatusFragment.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "ArcEnvironmentalStatusProcessor.generated.h"

UCLASS(meta = (DisplayName = "Arc Environmental Status Processor"))
class ARCWEATHER_API UArcEnvironmentalStatusProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcEnvironmentalStatusProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
	float SignalTimer = 0.f;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Environmental Status Trait"))
class ARCWEATHER_API UArcEnvironmentalStatusTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Environmental Status")
	FArcEnvironmentalStatusConfig StatusConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
