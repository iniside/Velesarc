// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"
#include "ArcMassAttachmentProcessor.generated.h"

// ---------------------------------------------------------------------------
// UArcMassAttachmentProcessor
//
// Updates the follower entity's FTransformFragment to LocalOffset *
// Target's FTransformFragment each tick. If Target is no longer valid,
// adds FArcMassAttachmentLostTag to the follower (deferred) so other
// systems can run cleanup. Runs on PrePhysics so visualization processors
// (PostPhysics, e.g. UArcNiagaraVisDynamicDataProcessor) read the updated
// transform.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassAttachmentProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassAttachmentProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery{*this};
};
