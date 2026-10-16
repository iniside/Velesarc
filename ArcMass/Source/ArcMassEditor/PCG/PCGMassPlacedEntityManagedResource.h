// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "PCGManagedResource.h"

class AArcPCGPlacedEntityPartitionActor;

#include "PCGMassPlacedEntityManagedResource.generated.h"

UCLASS(BlueprintType)
class ARCMASSEDITOR_API UPCGMassPlacedEntityManagedResource : public UPCGManagedResource
{
	GENERATED_BODY()

public:
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

	virtual bool Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	// ReleaseIfUnused: engine default fires (calls Release(true,...) when bIsMarkedUnused). No override needed.

	// PCG's OnComponentDestroyed does not call CleanupLocalImmediate, so deleting the PCG actor in the editor
	// would otherwise GC this resource without ever firing Release — leaving our orphan partition actors behind.
	// Override BeginDestroy so the resource's own teardown removes our entries from each touched actor.
	// Also unbinds the FEditorDelegates::OnDeleteActorsEnd hook installed in PostInitProperties.
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void ChangeTransientState(EPCGEditorDirtyMode NewEditingMode) override;
	virtual void MarkTransientOnLoad() override;

private:
	// Synchronous cleanup path: runs the same hard-release teardown BeginDestroy would, but
	// immediately on actor-delete instead of waiting for GC. Hooked from
	// FEditorDelegates::OnDeleteActorsEnd. Safe to call repeatedly — short-circuits when
	// bHardReleased is already true.
	void RunSyncCleanupIfOwnerGone();
#endif

public:
	// Stamped onto every FArcPlacedEntityEntry this resource creates. Set externally by the PCG element.
	UPROPERTY()
	FGuid OwnerGuid;

	// Stable identity of the originating UPCGSettings instance, used to find-or-create this
	// resource across PCG generations of the same node.
	UPROPERTY()
	uint64 SettingsStableUID = 0;

	// Actors we wrote entries into during the current/last generation.
	// Weak so a partition actor destroyed by us-or-others won't keep us alive.
	UPROPERTY()
	TArray<TWeakObjectPtr<AArcPCGPlacedEntityPartitionActor>> TouchedPartitionActors;

#if WITH_EDITORONLY_DATA
	// Transient: tracks the previous dirty mode so ChangeTransientState can gate on transitions.
	UPROPERTY(Transient)
	EPCGEditorDirtyMode LastKnownMode = EPCGEditorDirtyMode::Normal;

	/** Set to true once Release(bHardRelease=true) has fully run for this resource.
	 *  Prevents BeginDestroy from running cleanup a second time when GC tears down
	 *  the resource after the engine has already renamed it to the transient package
	 *  (PCGManagedResourceContainer.cpp:106-183, two rename-after-hard-release paths). */
	UPROPERTY(Transient)
	bool bHardReleased = false;

	// Handle for FEditorDelegates::OnDeleteActorsEnd. Bound in PostInitProperties, unbound in BeginDestroy.
	FDelegateHandle DeleteActorsEndHandle;
#endif
};
