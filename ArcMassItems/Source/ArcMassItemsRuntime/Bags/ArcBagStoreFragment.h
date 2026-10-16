// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityElementTypes.h"
#include "Replication/ArcIrisReplicatedArray.h"
#include "Bags/ArcBagId.h"
#include "Items/ArcItemId.h"
#include "ArcBagStoreFragment.generated.h"

class UArcBagDefinition;
struct FArcBagInstance;
struct FArcBagReplicatedArray;

/**
 * Local (non-replicating) notification raised when a replicated bag instance is
 * added, changed, or removed on a client.
 *
 * NOTE: this process-global static is a PLACEHOLDER for the deferred client
 * replication step (Step 8). When replication is actually wired up, world-context
 * routing MUST replace this static — under PIE a single process hosts both the
 * server and the client(s), so a process-global broadcast cannot distinguish
 * worlds. It is INERT in this rollout because FArcMassBagStoreFragment is not yet
 * listed in ReplicatedFragments, so the replication callbacks never fire.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FArcBagInstanceChangedSignature, const FArcBagInstance&);

/** Accessor for the placeholder bag-change broadcast (see note above). */
struct ARCMASSITEMSRUNTIME_API FArcBagInstanceChanges
{
	static FArcBagInstanceChangedSignature OnBagInstanceChanged;
};

/**
 * One bag overlay instance: a replication-stable identity plus the ordered item
 * slots it owns. Slots index into the entity's target item store (see
 * FArcMassBagStoreFragment::TargetStoreType).
 */
USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcBagInstance : public FArcIrisReplicatedArrayItem
{
	GENERATED_BODY()

	/** Stable per-instance identity. */
	UPROPERTY()
	FArcBagId BagId;

	/** Ordered item ids occupying this bag's slots (INDEX_NONE-equivalent = empty). */
	UPROPERTY()
	TArray<FArcItemId> Slots;

	/** Replication-stable definition identity — resolved to a definition on demand. */
	UPROPERTY()
	FPrimaryAssetId DefinitionId;

	/** Non-UPROPERTY resolved cache of DefinitionId. Resolved on demand; never serialized. */
	const UArcBagDefinition* CachedDefinition = nullptr;

	FArcBagInstance() = default;

	bool operator==(const FArcBagId& InBagId) const { return BagId == InBagId; }

	// Replication callbacks (signatures mirror FArcMassReplicatedItem). Each raises
	// the local bag-change notification so Step 6 listeners can bind.
	void PreReplicatedRemove(const FArcBagReplicatedArray& Array) { FArcBagInstanceChanges::OnBagInstanceChanged.Broadcast(*this); }
	void PostReplicatedAdd(const FArcBagReplicatedArray& Array) { FArcBagInstanceChanges::OnBagInstanceChanged.Broadcast(*this); }
	void PostReplicatedChange(const FArcBagReplicatedArray& Array) { FArcBagInstanceChanges::OnBagInstanceChanged.Broadcast(*this); }
};

/** Iris-replicated array of bag overlay instances. */
USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcBagReplicatedArray : public FArcIrisReplicatedArray
{
	GENERATED_BODY()

	FArcBagReplicatedArray() { InitCallbacks<FArcBagReplicatedArray, FArcBagInstance>(); }

	UPROPERTY()
	TArray<FArcBagInstance> Items;

	int32 AddItem(FArcBagInstance&& Item) { return FArcIrisReplicatedArray::AddItem(Items, MoveTemp(Item)); }
	void RemoveItemAt(int32 Index) { FArcIrisReplicatedArray::RemoveItemAt(Items, Index); }

	void MarkItemDirty(int32 Index) { MarkItemDirtyByIndex(Items[Index].ReplicationKey, Index, Items[Index].IrisRepID); }
	void MarkItemDirty(FArcBagInstance& Item) { FArcIrisReplicatedArray::MarkItemDirty(Items, Item); }
	void MarkArrayDirty() { FArcIrisReplicatedArray::MarkAllDirty(Items); }
	void ClearDirtyState() { FArcIrisReplicatedArray::ClearDirtyState(); }

	FArcBagInstance* FindById(FArcBagId BagId);
	int32 FindIndexById(FArcBagId BagId) const;
};

/**
 * Sparse fragment carrying the bag overlay for an entity: the set of bag instances
 * and the item store type their slots index into. Derive nothing — one overlay per
 * entity; the target store is named explicitly via TargetStoreType.
 */
USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassBagStoreFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	/** The item store struct type that this overlay's slots index into. */
	UPROPERTY()
	TObjectPtr<UScriptStruct> TargetStoreType;

	/** The replicated bag instances. */
	UPROPERTY()
	FArcBagReplicatedArray Bags;
};

template<>
struct TMassFragmentTraits<FArcMassBagStoreFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
