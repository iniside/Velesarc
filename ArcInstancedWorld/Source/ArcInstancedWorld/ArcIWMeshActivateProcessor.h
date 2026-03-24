// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSignalProcessorBase.h"
#include "ArcIWMeshActivateProcessor.generated.h"

// ---------------------------------------------------------------------------
// Mesh Activate Processor
// Subscribes to MeshCellActivated signal -- adds ISM instances (mesh-only).
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWMeshActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWMeshActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
