// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "ArcMassItemStoreTrait.generated.h"

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemStoreTag : public FMassTag
{
	GENERATED_BODY()
};

// Adds the default FArcMassItemStoreFragment sparse fragment to an entity.
// To support multiple stores on a single entity (e.g. a high-churn inventory store
// and a static equipment store), derive FArcMassItemStoreFragment, then add one
// instance of this trait per derived store type to the entity config.
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Item Store", Category = "Items"))
class ARCMASSITEMSRUNTIME_API UArcMassItemStoreTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassItemStoreInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassItemStoreInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
