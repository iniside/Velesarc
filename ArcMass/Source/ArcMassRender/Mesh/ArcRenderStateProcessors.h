// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/ObserverProcessor.h"
#include "MassSignalProcessorBase.h"

#include "ArcRenderStateProcessors.generated.h"

struct FMassEntityManager;
struct FMassExecutionContext;

/**
 * Processor responsible for updating or recreating the render state when dirtied
 * Listens to the following signals:
 * - UE::Mass::Signals::TransformChanged
 * - UE::Mass::Signals::MeshVisualPropertyChanged
 * - UE::Mass::Signals::MeshChanged
 * - UE::Mass::Signals::SelectionChanged
 * - UE::Mass::Signals::LevelEditingStateChanged
 * - UE::Mass::Signals::RenderStateDirty
 */
UCLASS()
class ARCMASSRENDER_API UArcRenderStateDirtyUpdateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcRenderStateDirtyUpdateProcessor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

/**
 * Creates render state for static-mesh entities when FArcRenderStateFragment is added
 * (replaces the create poll for the static path). ISM holder entities carry
 * FArcRenderISMFragment (not FArcRenderStaticMeshFragment), so the Presence::All requirement
 * on FArcRenderStaticMeshFragment structurally excludes them — no explicit tag or processor
 * reference needed.
 */
UCLASS()
class ARCMASSRENDER_API UArcCreateRenderStateObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcCreateRenderStateObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/**
 * Processor responsible for destroying the render states
 */
UCLASS()
class ARCMASSRENDER_API UArcDestroyRenderStateProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcDestroyRenderStateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
