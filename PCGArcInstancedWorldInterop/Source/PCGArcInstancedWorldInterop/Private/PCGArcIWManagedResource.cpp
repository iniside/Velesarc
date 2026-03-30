// Copyright Lukasz Baran. All Rights Reserved.

#include "PCGArcIWManagedResource.h"

#include "PCGArcIWInteropModule.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "Helpers/PCGHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGArcIWManagedResource)

void UPCGArcIWManagedResource::PostEditImport()
{
	Super::PostEditImport();
	TrackedEntries.Reset();
}

bool UPCGArcIWManagedResource::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGArcIWManagedResource::Release);

#if WITH_EDITOR
	if (PCGHelpers::IsRuntimeOrPIE())
	{
		return false;
	}

	for (FPCGArcIWTrackedPartitionEntry& Entry : TrackedEntries)
	{
		AArcIWMassISMPartitionActor* PartitionActor = Entry.PartitionActor;
		if (!IsValid(PartitionActor))
		{
			continue;
		}

		Entry.TransformIndices.Sort();
		PartitionActor->RemoveActorInstances(Entry.ActorClass, Entry.TransformIndices);

		if (PartitionActor->GetActorClassEntries().IsEmpty())
		{
			OutActorsToDelete.Add(PartitionActor);
		}
		else
		{
			PartitionActor->RebuildEditorPreviewISMCs();
			PartitionActor->Modify();
		}
	}

	TrackedEntries.Reset();
#endif

	return true;
}

bool UPCGArcIWManagedResource::ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	return TrackedEntries.IsEmpty() && Super::ReleaseIfUnused(OutActorsToDelete);
}

#if WITH_EDITOR
void UPCGArcIWManagedResource::MarkTransientOnLoad()
{
	UE_LOG(LogPCGArcIWInterop, Warning,
		TEXT("ArcIW PCG resources cannot be marked as transient on load."));
	Super::MarkTransientOnLoad();
}

void UPCGArcIWManagedResource::ChangeTransientState(EPCGEditorDirtyMode NewEditingMode)
{
	if (NewEditingMode != EPCGEditorDirtyMode::Normal)
	{
		UE_LOG(LogPCGArcIWInterop, Warning,
			TEXT("ArcIW PCG resources do not support preview mode. Flushing instances."));
		TSet<TSoftObjectPtr<AActor>> Dummy;
		Release(/*bHardRelease=*/true, Dummy);
	}

	Super::ChangeTransientState(NewEditingMode);
}
#endif
