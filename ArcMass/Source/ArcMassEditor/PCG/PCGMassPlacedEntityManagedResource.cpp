// Copyright Lukasz Baran. All Rights Reserved.

#include "PCG/PCGMassPlacedEntityManagedResource.h"

#include "ArcMass/PlacedEntities/ArcPCGPlacedEntityPartitionActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PCGComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMassPlacedEntityManagedResource)

void UPCGMassPlacedEntityManagedResource::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	// Hook editor actor-delete so cleanup runs synchronously on volume delete instead of
	// waiting for GC. GC interval in editor is on the order of tens of seconds; with
	// multiple graphs overlapping a partition cell, deferring cleanup that long opens
	// real race windows. BeginDestroy remains as the GC-fallback safety net.
	// Skip binding for CDO / archetype objects — they never own real partition actors.
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && GIsEditor)
	{
		DeleteActorsEndHandle = FEditorDelegates::OnDeleteActorsEnd.AddUObject(
			this, &UPCGMassPlacedEntityManagedResource::RunSyncCleanupIfOwnerGone);
	}
#endif
}

void UPCGMassPlacedEntityManagedResource::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// PIE duplication keeps the original GUID so the PIE copy refers to the same logical owner.
	// Any other duplication (copy-paste, Save-As, etc.) mints a fresh GUID.
	if (DuplicateMode != EDuplicateMode::PIE)
	{
		OwnerGuid = FGuid::NewGuid();
	}
}

void UPCGMassPlacedEntityManagedResource::BeginDestroy()
{
#if WITH_EDITOR
	if (DeleteActorsEndHandle.IsValid())
	{
		FEditorDelegates::OnDeleteActorsEnd.Remove(DeleteActorsEndHandle);
		DeleteActorsEndHandle.Reset();
	}

	// GC fallback. The OnDeleteActorsEnd hook in PostInitProperties usually wins (synchronous,
	// fires before GC), but we keep this for the non-editor-delete code paths that still let
	// GC tear the resource down — e.g. the engine renaming the resource to the transient
	// package after a hard Release (PCGManagedResourceContainer.cpp:106-183).
	// bHardReleased short-circuits this path when sync cleanup already ran.
	RunSyncCleanupIfOwnerGone();
#endif

	Super::BeginDestroy();
}

#if WITH_EDITOR
void UPCGMassPlacedEntityManagedResource::RunSyncCleanupIfOwnerGone()
{
	const bool bSafeToCleanup =
		!IsEngineExitRequested()
		&& !GExitPurge
		&& !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
		&& !bHardReleased;

	if (!bSafeToCleanup)
	{
		return;
	}

	// Distinguish "the actor that owned this resource is genuinely being deleted"
	// from "the outer component is being recreated under us" (BP reconstruction /
	// undo / PIE duplication). On a real actor-delete the outer chain stays intact
	// and the actor's IsActorBeingDestroyed() returns true (set synchronously in
	// AActor::Destroy before the deferred GC sweep). On BP reconstruction the actor
	// persists with IsActorBeingDestroyed() == false. Default to "do nothing" if we
	// can't positively confirm actor destruction — worst case is partition actors
	// stay around until the next regen's wipe-on-reuse path runs, which is
	// preferable to spuriously destroying live content.
	UPCGComponent* OwnerComponent = GetTypedOuter<UPCGComponent>();
	AActor* OwnerActor = OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
	const bool bOwnerActorGoingAway =
		IsValid(OwnerActor) && OwnerActor->IsActorBeingDestroyed();

	if (!bOwnerActorGoingAway)
	{
		return;
	}

	TSet<TSoftObjectPtr<AActor>> ActorsToDelete;
	Release(/*bHardRelease=*/true, ActorsToDelete);

	for (const TSoftObjectPtr<AActor>& SoftActor : ActorsToDelete)
	{
		AActor* Actor = SoftActor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}
		UWorld* World = Actor->GetWorld();
		if (World != nullptr && !World->bIsTearingDown)
		{
			World->DestroyActor(Actor);
		}
	}
}
#endif

bool UPCGMassPlacedEntityManagedResource::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMassPlacedEntityManagedResource::Release);

	if (!bHardRelease)
	{
		// Soft release: mark unused so the engine can either reclaim us (MarkAsUsed/MarkAsReused)
		// or hard-kill us (ReleaseIfUnused) after generation completes.
		bIsMarkedUnused = true;
		return false;
	}

	// Hard release: remove our entries from every partition actor we wrote into.
#if WITH_EDITOR
	for (TWeakObjectPtr<AArcPCGPlacedEntityPartitionActor>& WeakActor : TouchedPartitionActors)
	{
		AArcPCGPlacedEntityPartitionActor* Actor = WeakActor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		const bool bActorNowEmpty = Actor->RemoveEntriesOwnedBy(OwnerGuid);
		if (bActorNowEmpty && !Actor->HasHandPlacedEntries())
		{
			OutActorsToDelete.Add(TSoftObjectPtr<AActor>(Actor));
		}
	}

#endif // WITH_EDITOR
#if WITH_EDITORONLY_DATA
	bHardReleased = true;
#endif // WITH_EDITORONLY_DATA

	TouchedPartitionActors.Reset();
	return true;
}

#if WITH_EDITOR
void UPCGMassPlacedEntityManagedResource::MarkTransientOnLoad()
{
	// Entries persist on the partition actor via WP-cell save; no warning needed here.
	Super::MarkTransientOnLoad();
}

void UPCGMassPlacedEntityManagedResource::ChangeTransientState(EPCGEditorDirtyMode NewEditingMode)
{
	// Only hard-release when transitioning INTO Preview from Normal.
	// Returning to Normal is a no-op — the next generation will re-emit entries.
	const bool bEnteringPreview =
		(NewEditingMode == EPCGEditorDirtyMode::Preview || NewEditingMode == EPCGEditorDirtyMode::LoadAsPreview)
		&& LastKnownMode == EPCGEditorDirtyMode::Normal;

	if (bEnteringPreview)
	{
		TSet<TSoftObjectPtr<AActor>> Dummy;
		Release(/*bHardRelease=*/true, Dummy);
	}

	LastKnownMode = NewEditingMode;

	Super::ChangeTransientState(NewEditingMode);
}
#endif
