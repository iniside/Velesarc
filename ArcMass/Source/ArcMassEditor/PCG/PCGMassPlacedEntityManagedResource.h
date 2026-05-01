// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGManagedResource.h"
#include "PCGMassPlacedEntityManagedResource.generated.h"

class AArcPCGPlacedEntityPartitionActor;

UCLASS(MinimalAPI, BlueprintType)
class UPCGMassPlacedEntityManagedResource : public UPCGManagedResource
{
	GENERATED_BODY()

public:
	virtual void PostEditImport() override;

	virtual bool Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	virtual bool ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;

#if WITH_EDITOR
	virtual void ChangeTransientState(EPCGEditorDirtyMode NewEditingMode) override;
	virtual void MarkTransientOnLoad() override;
#endif

public:
	UPROPERTY()
	TArray<TObjectPtr<AArcPCGPlacedEntityPartitionActor>> TrackedActors;
};
