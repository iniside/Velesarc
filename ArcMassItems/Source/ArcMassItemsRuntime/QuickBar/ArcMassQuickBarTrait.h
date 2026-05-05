// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "ArcMassQuickBarTrait.generated.h"

class UArcMassQuickBarConfig;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc QuickBar", Category = "Items"))
class ARCMASSITEMSRUNTIME_API UArcMassQuickBarTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "QuickBar")
	TObjectPtr<UArcMassQuickBarConfig> QuickBarConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassQuickBarStateInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassQuickBarStateInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
