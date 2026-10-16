// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/ObserverProcessor.h"
#include "MassSignalProcessorBase.h"

#include "ArcRenderStaticMeshProcessors.generated.h"

struct FMassEntityManager;
struct FMassExecutionContext;

/**
 * Base processor responsible for setting up the static mesh render state for the renderer
 */
UCLASS(Abstract)
class ARCMASSRENDER_API UArcBaseStaticMeshSetupRenderStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBaseStaticMeshSetupRenderStateProcessor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	FMassEntityQuery EntityQuery;
};

/**
 * Processor responsible for setting up the static mesh render state for the renderer
 */
UCLASS()
class ARCMASSRENDER_API UArcStaticMeshSetupRenderStateProcessor : public UArcBaseStaticMeshSetupRenderStateProcessor
{
	GENERATED_BODY()

public:
	UArcStaticMeshSetupRenderStateProcessor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext) override;
};

/**
 * Processor responsible for updating the static mesh render state when dirtied
 * Listening the following signals on the entity
 * - UE::Mass::Signals::TransformChanged
 * - UE::Mass::Signals::MeshVisualPropertyChanged
 * - UE::Mass::Signals::MeshChanged
 * - UE::Mass::Signals::SelectionChanged
 * - UE::Mass::Signals::LevelEditingStateChanged
 */
UCLASS()
class ARCMASSRENDER_API UArcStaticMeshUpdateRenderStateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcStaticMeshUpdateRenderStateProcessor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};


/**
 * Base processor responsible for removing the common static mesh render state fragments when the required fragments are being removed
 */
UCLASS(Abstract)
class ARCMASSRENDER_API UArcBaseStaticMeshDestroyRenderStateProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcBaseStaticMeshDestroyRenderStateProcessor();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager);

protected:
	FMassEntityQuery EntityQuery;
};

/**
 * Processor responsible for removing the static mesh render state fragments when the required fragments are being removed
 */
UCLASS()
class ARCMASSRENDER_API UArcStaticMeshDestroyRenderStateProcessor : public UArcBaseStaticMeshDestroyRenderStateProcessor
{
	GENERATED_BODY()

public:
	UArcStaticMeshDestroyRenderStateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
};
