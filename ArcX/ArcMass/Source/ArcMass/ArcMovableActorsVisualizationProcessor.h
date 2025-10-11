// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityElementTypes.h"
#include "MassLODCollectorProcessor.h"

#include "MassRepresentationProcessor.h"
#include "MassVisualizationLODProcessor.h"

#include "ArcMovableActorsVisualizationProcessor.generated.h"

USTRUCT()
struct ARCMASS_API FArcMovableActorsVisualizationProcessorTag : public FMassTag
{
	GENERATED_BODY();
};

/**
 * 
 */
UCLASS()
class ARCMASS_API UArcMovableActorsVisualizationProcessor : public UMassVisualizationProcessor
{
	GENERATED_BODY()

public:
	UArcMovableActorsVisualizationProcessor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
};

UCLASS()
class ARCMASS_API UArcMovableActorsVisualizationLODProcessor: public UMassVisualizationLODProcessor
{
	GENERATED_BODY()

public:
	UArcMovableActorsVisualizationLODProcessor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
};


UCLASS()
class ARCMASS_API UArcMovableActorsLODCollectorProcessor: public UMassLODCollectorProcessor
{
	GENERATED_BODY()

public:
	UArcMovableActorsLODCollectorProcessor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
};
