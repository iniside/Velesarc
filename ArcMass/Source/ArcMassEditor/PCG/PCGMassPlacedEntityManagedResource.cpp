// Copyright Lukasz Baran. All Rights Reserved.

#include "PCG/PCGMassPlacedEntityManagedResource.h"

#include "PCG/ArcPCGPlacedEntityPartitionActor.h"
#include "Helpers/PCGHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMassPlacedEntityManagedResource)

void UPCGMassPlacedEntityManagedResource::PostEditImport()
{
	Super::PostEditImport();
	TrackedActors.Reset();
}

bool UPCGMassPlacedEntityManagedResource::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMassPlacedEntityManagedResource::Release);

#if WITH_EDITOR
	if (PCGHelpers::IsRuntimeOrPIE())
	{
		return false;
	}

	for (AArcPCGPlacedEntityPartitionActor* Actor : TrackedActors)
	{
		if (IsValid(Actor))
		{
			OutActorsToDelete.Add(Actor);
		}
	}

	TrackedActors.Reset();
#endif

	return true;
}

bool UPCGMassPlacedEntityManagedResource::ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	return TrackedActors.IsEmpty() && Super::ReleaseIfUnused(OutActorsToDelete);
}

#if WITH_EDITOR
void UPCGMassPlacedEntityManagedResource::MarkTransientOnLoad()
{
	UE_LOG(LogTemp, Warning,
		TEXT("PCG Mass placed entity resources cannot be marked as transient on load."));
	Super::MarkTransientOnLoad();
}

void UPCGMassPlacedEntityManagedResource::ChangeTransientState(EPCGEditorDirtyMode NewEditingMode)
{
	if (NewEditingMode != EPCGEditorDirtyMode::Normal)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PCG Mass placed entity resources do not support preview mode. Flushing instances."));
		TSet<TSoftObjectPtr<AActor>> Dummy;
		Release(/*bHardRelease=*/true, Dummy);
	}

	Super::ChangeTransientState(NewEditingMode);
}
#endif
