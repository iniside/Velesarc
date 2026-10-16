// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Mass/ObserverProcessor.h"
#include "Mass/Processor.h"
#include "MassEntityTraitBase.h"
#include "ArcInfluenceInvoker.generated.h"

/** Per-entity state that keeps tiles of a tiled influence grid allocated around a moving entity. */
USTRUCT()
struct ARCMASS_API FArcInfluenceInvokerFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Tiles overlapping this radius are allocated and kept alive. */
	UPROPERTY(EditAnywhere, Category = "InfluenceInvoker", meta = (ClampMin = "1.0"))
	float Radius = 3000.f;

	/** Distance past Radius a held tile survives, so an entity pacing a tile edge does not thrash allocation. */
	UPROPERTY(EditAnywhere, Category = "InfluenceInvoker", meta = (ClampMin = "0.0"))
	float ReleaseMargin = 1000.f;

	/** Index into UArcInfluenceSettings::Grids. Ignored unless that grid is tiled. */
	UPROPERTY(EditAnywhere, Category = "InfluenceInvoker", meta = (ClampMin = "0"))
	int32 GridIndex = 1;

	/** Tile coordinates this invoker currently holds a ref count on. */
	TArray<FIntPoint, TInlineAllocator<9>> HeldTiles;

	FIntPoint LastCell = FIntPoint::ZeroValue;

	bool bFootprintValid = false;
};

template<>
struct TMassFragmentTraits<FArcInfluenceInvokerFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Makes an entity keep tiles of a tiled influence grid allocated around itself. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Influence Invoker"))
class ARCMASS_API UArcInfluenceInvokerTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "InfluenceInvoker", meta = (ClampMin = "1.0"))
	float Radius = 3000.f;

	UPROPERTY(EditAnywhere, Category = "InfluenceInvoker", meta = (ClampMin = "0.0"))
	float ReleaseMargin = 1000.f;

	UPROPERTY(EditAnywhere, Category = "InfluenceInvoker", meta = (ClampMin = "0"))
	int32 GridIndex = 1;
};

/**
 * Applies tile ref-count deltas for invokers that crossed a grid cell boundary,
 * within a per-tick budget.
 */
UCLASS()
class ARCMASS_API UArcInfluenceInvokerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcInfluenceInvokerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	/** Number of pending invokers skipped at the start of a pass, so a saturated budget stays fair. */
	int32 PendingCursor = 0;

	bool bLoggedNonTiledGrid = false;
};

/** Releases the tiles an invoker held when its fragment goes away. */
UCLASS()
class ARCMASS_API UArcInfluenceInvokerRemovalObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcInfluenceInvokerRemovalObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
