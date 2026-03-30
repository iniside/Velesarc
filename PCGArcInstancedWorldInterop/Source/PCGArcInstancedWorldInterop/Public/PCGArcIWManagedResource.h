// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGManagedResource.h"

#include "PCGArcIWManagedResource.generated.h"

class AArcIWMassISMPartitionActor;

USTRUCT()
struct FPCGArcIWTrackedPartitionEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AArcIWMassISMPartitionActor> PartitionActor;

	UPROPERTY()
	TSubclassOf<AActor> ActorClass;

	UPROPERTY()
	TArray<int32> TransformIndices;
};

UCLASS(MinimalAPI, BlueprintType)
class UPCGArcIWManagedResource : public UPCGManagedResource
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
	TArray<FPCGArcIWTrackedPartitionEntry> TrackedEntries;
};
