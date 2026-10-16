// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Elements/SMInstance/SMInstanceManager.h"
#include "StructUtils/InstancedStruct.h"
#include "Containers/Ticker.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementId.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityOverrideHost.h"
#include "ArcPlacedEntityProxyEditingObject.generated.h"

class AArcPlacedEntityPartitionActor;
class IArcMassEntityManager;
struct FArcPlacedEntityEntry;

UENUM()
enum class EArcPlacedEntityDetailView : uint8
{
	Fragments,
	Traits
};

/**
 * Proxy editing object for per-instance fragment overrides on placed Mass entities.
 * Created by the TypedElements framework when an ISM instance is selected in the viewport.
 * Holds editable fragment copies and syncs them to/from the partition actor's override map.
 */
UCLASS(Transient)
class UArcPlacedEntityProxyEditingObject : public USMInstanceProxyEditingObject
{
	GENERATED_BODY()

public:
	virtual void Shutdown() override;

	/**
	 * Initialize from a componentless MassEntity element ID.
	 * Supports any owner implementing IArcPlacedEntityOverrideHost (AArcPlacedEntityPartitionActor and AArcAlwaysLoadedEntityActor).
	 */
	void InitFromMassEntityElement(const FArcMassEntityElementId& InId);

	virtual void BeginDestroy() override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

	UPROPERTY(EditAnywhere, Category = "Transform", meta = (ShowOnlyInnerProperties))
	FTransform Transform;

	UPROPERTY(VisibleAnywhere, Category = "Entity")
	FString EntityDisplayName;

	UPROPERTY(EditAnywhere, Category = "View")
	EArcPlacedEntityDetailView DetailView = EArcPlacedEntityDetailView::Fragments;

	/** Fragment overrides currently being edited. Populated in Initialize(). */
	TArray<FInstancedStruct> EditableFragments;

	/** Display names for each editable fragment (parallel to EditableFragments). */
	TArray<FString> FragmentDisplayNames;

	/** True if the proxy has a resolved owner actor and valid indices. */
	bool IsValid() const { return (PartitionActor.IsValid() || OverrideHostObject.IsValid()) && EntryIndex != INDEX_NONE; }

	int32 GetEntryIndex() const { return EntryIndex; }
	int32 GetTransformIndex() const { return TransformIndex; }
	AArcPlacedEntityPartitionActor* GetPartitionActor() const { return PartitionActor.Get(); }

	/**
	 * Returns a pointer to the active FArcPlacedEntityEntry for this proxy (using the active owner's entry list
	 * and EntryIndex). Returns nullptr if the owner or index is invalid.
	 */
	const FArcPlacedEntityEntry* GetActiveEntry() const;

	/** Returns the world from the active owner UObject. */
	UWorld* GetActiveOwnerWorld() const;

	/** Write current EditableFragments back to the owner's override map. */
	void WriteOverridesToPartitionActor();

private:
	virtual bool SyncProxyStateFromInstance() override;

	/**
	 * Resolve OverrideHostObject/OverrideHost/EntryIndex/TransformIndex from a MassEntity element ID.
	 * Also sets PartitionActor if the host is AArcPlacedEntityPartitionActor (for backwards compat).
	 * Returns false if the owner does not implement IArcPlacedEntityOverrideHost.
	 */
	bool ResolveToPartitionActorFromMassEntity(const FArcMassEntityElementId& InId);

	/** Returns the entry array from the active owner (PartitionActor for ISM/SkM paths, OverrideHost for MassEntity path). */
	TArray<FArcPlacedEntityEntry>* GetActiveEntries();
	const TArray<FArcPlacedEntityEntry>* GetActiveEntries() const;

	/** Calls Modify() on the active owner UObject. */
	void ModifyActiveOwner();

	void BuildEditableFragmentList();

	/** Update EntityDisplayName from the active entry's EntityConfig and TransformIndex. */
	void UpdateDisplayName();

	/** Extract ticker setup so Initialize and InitFromMassEntityElement can share it. */
	void StartSyncTicker();

	// MassEntity path flag — set by InitFromMassEntityElement on successful resolve
	bool bIsMassEntity = false;

	/** Build a FArcMassEntityElementId from the current OverrideHostObject/EntryIndex/TransformIndex. */
	FArcMassEntityElementId GetMassEntityId() const;

	/**
	 * Get the IArcMassEntityManager for the current OverrideHostObject.
	 * Both AArcPlacedEntityPartitionActor and AArcAlwaysLoadedEntityActor implement IArcMassEntityManager
	 * directly, so a direct cast resolves the same manager that the element id expects.
	 */
	IArcMassEntityManager* GetMassEntityManager() const;

	// ISM/SkM path: partition actor (typed for backwards compat with callers of GetPartitionActor())
	TWeakObjectPtr<AArcPlacedEntityPartitionActor> PartitionActor;

	// MassEntity path: any actor implementing IArcPlacedEntityOverrideHost (may also be the partition actor)
	TWeakObjectPtr<UObject> OverrideHostObject;
	IArcPlacedEntityOverrideHost* OverrideHost = nullptr;

	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;

	FTSTicker::FDelegateHandle TickHandle;
	bool bIsWithinInteractiveTransformEdit = false;
};
