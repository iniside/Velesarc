// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Containers/ArrayView.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementId.h"
#include "ArcMassEntityManager.generated.h"

enum class ETypedElementWorldType : uint8;

/**
 * An interface for actors that manage Mass entity instances.
 * This exists so that actors that directly manage instances can inject custom logic
 * around the manipulation of the underlying partition actor data.
 * @note The Mass entity instances given to this API must be valid and belong to this
 *       instance manager. The implementation is free to assert or crash if that contract is broken.
 */
UINTERFACE(MinimalAPI)
class UArcMassEntityManager : public UInterface
{
	GENERATED_BODY()
};

class IArcMassEntityManager
{
	GENERATED_BODY()

public:
	/**
	 * Get the display name of the given Mass entity instance.
	 */
	virtual FText GetMassEntityInstanceDisplayName(const FArcMassEntityElementId& InId) const
	{
		return FText();
	}

	/**
	 * Get the tooltip of the given Mass entity instance.
	 */
	virtual FText GetMassEntityInstanceTooltip(const FArcMassEntityElementId& InId) const
	{
		return FText();
	}

	/**
	 * Can the given Mass entity instance be edited?
	 * @return True if it can be edited, false otherwise.
	 */
	virtual bool CanEditMassEntityInstance(const FArcMassEntityElementId& InId) const = 0;

	/**
	 * Can the given Mass entity instance be moved in the world?
	 * @return True if it can be moved, false otherwise.
	 */
	virtual bool CanMoveMassEntityInstance(const FArcMassEntityElementId& InId, ETypedElementWorldType WorldType) const = 0;

	/**
	 * Attempt to get the transform of the given Mass entity instance.
	 * @note The transform is in the local space of the owner actor unless bWorldSpace is set.
	 * @return True if the transform was retrieved, false otherwise.
	 */
	virtual bool GetMassEntityInstanceTransform(const FArcMassEntityElementId& InId, FTransform& OutTransform, bool bWorldSpace = false) const = 0;

	/**
	 * Attempt to set the transform of the given Mass entity instance.
	 * @note The transform is in the local space of the owner actor unless bWorldSpace is set.
	 * @return True if the transform was updated, false otherwise.
	 */
	virtual bool SetMassEntityInstanceTransform(const FArcMassEntityElementId& InId, const FTransform& InTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) = 0;

	/**
	 * Notify that the given Mass entity instance is about to be moved.
	 */
	virtual void NotifyMassEntityInstanceMovementStarted(const FArcMassEntityElementId& InId) = 0;

	/**
	 * Notify that the given Mass entity instance is currently being moved.
	 */
	virtual void NotifyMassEntityInstanceMovementOngoing(const FArcMassEntityElementId& InId) = 0;

	/**
	 * Notify that the given Mass entity instance is done being moved.
	 */
	virtual void NotifyMassEntityInstanceMovementEnded(const FArcMassEntityElementId& InId) = 0;

	/**
	 * Notify that the given Mass entity instance selection state has changed.
	 */
	virtual void NotifyMassEntityInstanceSelectionChanged(const FArcMassEntityElementId& InId, bool bIsSelected) = 0;

	/**
	 * Enumerate every Mass entity instance element within the selection group that the given
	 * instance belongs to (including the given instance itself).
	 * A selection group allows disparate instances to be treated as a single logical unit for selection.
	 */
	virtual void ForEachMassEntityInstanceInSelectionGroup(const FArcMassEntityElementId& InId, TFunctionRef<bool(FArcMassEntityElementId)> Callback)
	{
		Callback(InId);
	}

	/**
	 * Can the given Mass entity instance be deleted?
	 * @return True if it can be deleted, false otherwise.
	 */
	virtual bool CanDeleteMassEntityInstance(const FArcMassEntityElementId& InId) const
	{
		return CanEditMassEntityInstance(InId);
	}

	/**
	 * Attempt to delete the given Mass entity instances.
	 * @return True if any instances were deleted, false otherwise.
	 */
	virtual bool DeleteMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds) = 0;

	/**
	 * Can the given Mass entity instance be duplicated?
	 * @return True if it can be duplicated, false otherwise.
	 */
	virtual bool CanDuplicateMassEntityInstance(const FArcMassEntityElementId& InId) const
	{
		return CanEditMassEntityInstance(InId);
	}

	/**
	 * Attempt to duplicate the given Mass entity instances, retrieving the IDs of any new instances.
	 * @return True if any instances were duplicated, false otherwise.
	 */
	virtual bool DuplicateMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds, TArray<FArcMassEntityElementId>& OutNewIds) = 0;

	/**
	 * Attempt to get the mesh-accurate world-space bounds for the given Mass entity instance.
	 * @return True if bounds were computed from the entry mesh, false if the entry has no mesh trait (caller should fall back).
	 */
#if WITH_EDITOR
	virtual bool GetMassEntityInstanceMeshBounds(const FArcMassEntityElementId& InId, FBoxSphereBounds& OutBounds) const = 0;
#endif // WITH_EDITOR
};

/**
 * An interface for actors that can provide a manager for Mass entity instances.
 */
UINTERFACE(MinimalAPI)
class UArcMassEntityManagerProvider : public UInterface
{
	GENERATED_BODY()
};

class IArcMassEntityManagerProvider
{
	GENERATED_BODY()

public:
	/**
	 * Attempt to get the instance manager associated with the given Mass entity instance, if any.
	 * @return The instance manager, or null if there is no manager for the given instance.
	 */
	virtual IArcMassEntityManager* GetMassEntityManager(const FArcMassEntityElementId& InId) = 0;
};

/**
 * A Mass entity instance manager, tied to a given Mass entity element ID.
 * Wraps an IArcMassEntityManager pointer and an FArcMassEntityElementId for convenient forwarding calls.
 */
struct ARCMASS_API FArcMassEntityManager
{
public:
	FArcMassEntityManager() = default;

	FArcMassEntityManager(const FArcMassEntityElementId& InInstanceId, IArcMassEntityManager* InInstanceManager)
		: InstanceId(InInstanceId)
		, InstanceManager(InInstanceManager)
	{
	}

	explicit operator bool() const
	{
		return InstanceId
			&& InstanceManager != nullptr;
	}

	bool operator==(const FArcMassEntityManager& InRHS) const
	{
		return InstanceId == InRHS.InstanceId
			&& InstanceManager == InRHS.InstanceManager;
	}

	bool operator!=(const FArcMassEntityManager& InRHS) const
	{
		return !(*this == InRHS);
	}

	friend inline uint32 GetTypeHash(const FArcMassEntityManager& InId)
	{
		return GetTypeHash(InId.InstanceId);
	}

	const FArcMassEntityElementId& GetInstanceId() const { return InstanceId; }
	IArcMassEntityManager* GetInstanceManager() const { return InstanceManager; }

	//~ IArcMassEntityManager interface forwarding
	FText GetMassEntityInstanceDisplayName() const { return InstanceManager->GetMassEntityInstanceDisplayName(InstanceId); }
	FText GetMassEntityInstanceTooltip() const { return InstanceManager->GetMassEntityInstanceTooltip(InstanceId); }
	bool CanEditMassEntityInstance() const { return InstanceManager->CanEditMassEntityInstance(InstanceId); }
	bool CanMoveMassEntityInstance(ETypedElementWorldType WorldType) const { return InstanceManager->CanMoveMassEntityInstance(InstanceId, WorldType); }
	bool GetMassEntityInstanceTransform(FTransform& OutTransform, bool bWorldSpace = false) const { return InstanceManager->GetMassEntityInstanceTransform(InstanceId, OutTransform, bWorldSpace); }
	bool SetMassEntityInstanceTransform(const FTransform& InTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) { return InstanceManager->SetMassEntityInstanceTransform(InstanceId, InTransform, bWorldSpace, bMarkRenderStateDirty, bTeleport); }
	void NotifyMassEntityInstanceMovementStarted() { InstanceManager->NotifyMassEntityInstanceMovementStarted(InstanceId); }
	void NotifyMassEntityInstanceMovementOngoing() { InstanceManager->NotifyMassEntityInstanceMovementOngoing(InstanceId); }
	void NotifyMassEntityInstanceMovementEnded() { InstanceManager->NotifyMassEntityInstanceMovementEnded(InstanceId); }
	void NotifyMassEntityInstanceSelectionChanged(bool bIsSelected) { InstanceManager->NotifyMassEntityInstanceSelectionChanged(InstanceId, bIsSelected); }
	void ForEachMassEntityInstanceInSelectionGroup(TFunctionRef<bool(FArcMassEntityElementId)> Callback) { InstanceManager->ForEachMassEntityInstanceInSelectionGroup(InstanceId, Callback); }
	bool CanDeleteMassEntityInstance() const { return InstanceManager->CanDeleteMassEntityInstance(InstanceId); }
	bool DeleteMassEntityInstance() { return InstanceManager->DeleteMassEntityInstances(MakeArrayView(&InstanceId, 1)); }
	bool CanDuplicateMassEntityInstance() const { return InstanceManager->CanDuplicateMassEntityInstance(InstanceId); }
#if WITH_EDITOR
	bool GetMassEntityInstanceMeshBounds(FBoxSphereBounds& OutBounds) const { return InstanceManager->GetMassEntityInstanceMeshBounds(InstanceId, OutBounds); }
#endif // WITH_EDITOR
	bool DuplicateMassEntityInstance(FArcMassEntityElementId& OutNewId)
	{
		TArray<FArcMassEntityElementId> NewIds;
		if (InstanceManager->DuplicateMassEntityInstances(MakeArrayView(&InstanceId, 1), NewIds))
		{
			OutNewId = NewIds[0];
			return true;
		}
		return false;
	}

private:
	FArcMassEntityElementId InstanceId;
	IArcMassEntityManager* InstanceManager = nullptr;
};
