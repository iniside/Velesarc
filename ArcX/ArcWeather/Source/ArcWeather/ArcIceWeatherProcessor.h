// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcWeatherSubsystem.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "ArcIceWeatherProcessor.generated.h"

struct FArcMassHealthFragment;

UCLASS(meta = (DisplayName = "Arc Ice Weather Processor"))
class ARCWEATHER_API UArcIceWeatherProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcIceWeatherProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
	float CurrentSignalTime = 0;
};

// Trait to add ice fragment + health fragment to an entity template
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Ice Trait"))
class ARCWEATHER_API UArcIceTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FArcIceFragment IceConfig;
	
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
