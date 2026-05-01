// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Containers/ArrayView.h"
#include "Templates/SubclassOf.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementId.h"
#include "SkMInstanceManager.generated.h"

enum class ETypedElementWorldType : uint8;
class USkMInstanceProxyEditingObject;

/**
 * An interface for actors that manage skinned mesh instances.
 * This exists so that actors that directly manage instances can inject custom logic
 * around the manipulation of the underlying ISMC component.
 * @note The skinned mesh instances given to this API must be valid and belong to this
 *       instance manager. The implementation is free to assert or crash if that contract is broken.
 */
UINTERFACE(MinimalAPI)
class USkMInstanceManager : public UInterface
{
	GENERATED_BODY()
};

class ISkMInstanceManager
{
	GENERATED_BODY()

public:
	/**
	 * Get the display name of the given skinned mesh instance.
	 */
	virtual FText GetSkMInstanceDisplayName(const FSkMInstanceId& InstanceId) const
	{
		return FText();
	}

	/**
	 * Get the tooltip of the given skinned mesh instance.
	 */
	virtual FText GetSkMInstanceTooltip(const FSkMInstanceId& InstanceId) const
	{
		return FText();
	}

	/**
	 * Can the given skinned mesh instance be edited?
	 * @return True if it can be edited, false otherwise.
	 */
	virtual bool CanEditSkMInstance(const FSkMInstanceId& InstanceId) const = 0;

	/**
	 * Can the given skinned mesh instance be moved in the world?
	 * @return True if it can be moved, false otherwise.
	 */
	virtual bool CanMoveSkMInstance(const FSkMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const = 0;

	/**
	 * Attempt to get the transform of the given skinned mesh instance.
	 * @note The transform is in the local space of the owner component unless bWorldSpace is set.
	 * @return True if the transform was retrieved, false otherwise.
	 */
	virtual bool GetSkMInstanceTransform(const FSkMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace = false) const = 0;

	/**
	 * Attempt to set the transform of the given skinned mesh instance.
	 * @note The transform is in the local space of the owner component unless bWorldSpace is set.
	 * @return True if the transform was updated, false otherwise.
	 */
	virtual bool SetSkMInstanceTransform(const FSkMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) = 0;

	/**
	 * Notify that the given skinned mesh instance is about to be moved.
	 */
	virtual void NotifySkMInstanceMovementStarted(const FSkMInstanceId& InstanceId) = 0;

	/**
	 * Notify that the given skinned mesh instance is currently being moved.
	 */
	virtual void NotifySkMInstanceMovementOngoing(const FSkMInstanceId& InstanceId) = 0;

	/**
	 * Notify that the given skinned mesh instance is done being moved.
	 */
	virtual void NotifySkMInstanceMovementEnded(const FSkMInstanceId& InstanceId) = 0;

	/**
	 * Notify that the given skinned mesh instance selection state has changed.
	 */
	virtual void NotifySkMInstanceSelectionChanged(const FSkMInstanceId& InstanceId, const bool bIsSelected) = 0;

	/**
	 * Enumerate every skinned mesh instance element within the selection group that the given
	 * instance belongs to (including the given instance itself).
	 * A selection group allows disparate instances to be treated as a single logical unit for selection.
	 */
	virtual void ForEachSkMInstanceInSelectionGroup(const FSkMInstanceId& InstanceId, TFunctionRef<bool(FSkMInstanceId)> Callback)
	{
		Callback(InstanceId);
	}

	/**
	 * Can the given skinned mesh instance be deleted?
	 * @return True if it can be deleted, false otherwise.
	 */
	virtual bool CanDeleteSkMInstance(const FSkMInstanceId& InstanceId) const
	{
		return CanEditSkMInstance(InstanceId);
	}

	/**
	 * Attempt to delete the given skinned mesh instances.
	 * @return True if any instances were deleted, false otherwise.
	 */
	virtual bool DeleteSkMInstances(TArrayView<const FSkMInstanceId> InstanceIds) = 0;

	/**
	 * Can the given skinned mesh instance be duplicated?
	 * @return True if it can be duplicated, false otherwise.
	 */
	virtual bool CanDuplicateSkMInstance(const FSkMInstanceId& InstanceId) const
	{
		return CanEditSkMInstance(InstanceId);
	}

	/**
	 * Attempt to duplicate the given skinned mesh instances, retrieving the IDs of any new instances.
	 * @return True if any instances were duplicated, false otherwise.
	 */
	virtual bool DuplicateSkMInstances(TArrayView<const FSkMInstanceId> InstanceIds, TArray<FSkMInstanceId>& OutNewInstanceIds) = 0;

	/**
	 * Optionally specify a different proxy object class for representing this SkM instance.
	 */
	virtual TSubclassOf<USkMInstanceProxyEditingObject> GetSkMInstanceEditingProxyClass() const
	{
		return nullptr;
	}
};

/**
 * An interface for actors that can provide a manager for skinned mesh instances.
 */
UINTERFACE(MinimalAPI)
class USkMInstanceManagerProvider : public UInterface
{
	GENERATED_BODY()
};

class ISkMInstanceManagerProvider
{
	GENERATED_BODY()

public:
	/**
	 * Attempt to get the instance manager associated with the given skinned mesh instance, if any.
	 * @return The instance manager, or null if there is no manager for the given instance.
	 */
	virtual ISkMInstanceManager* GetSkMInstanceManager(const FSkMInstanceId& InstanceId) = 0;
};

/**
 * A skinned mesh instance manager, tied to a given skinned mesh instance ID.
 * Wraps an ISkMInstanceManager pointer and an FSkMInstanceId for convenient forwarding calls.
 */
struct ARCMASS_API FSkMInstanceManager
{
public:
	FSkMInstanceManager() = default;

	FSkMInstanceManager(const FSkMInstanceId& InInstanceId, ISkMInstanceManager* InInstanceManager)
		: InstanceId(InInstanceId)
		, InstanceManager(InInstanceManager)
	{
	}

	explicit operator bool() const
	{
		return InstanceId
			&& InstanceManager != nullptr;
	}

	bool operator==(const FSkMInstanceManager& InRHS) const
	{
		return InstanceId == InRHS.InstanceId
			&& InstanceManager == InRHS.InstanceManager;
	}

	bool operator!=(const FSkMInstanceManager& InRHS) const
	{
		return !(*this == InRHS);
	}

	friend inline uint32 GetTypeHash(const FSkMInstanceManager& InId)
	{
		return GetTypeHash(InId.InstanceId);
	}

	const FSkMInstanceId& GetInstanceId() const { return InstanceId; }
	ISkMInstanceManager* GetInstanceManager() const { return InstanceManager; }

	UInstancedSkinnedMeshComponent* GetSkMComponent() const { return InstanceId.SkMComponent; }
	int32 GetPrimitiveInstanceIndex() const { return InstanceId.InstanceId; }

	//~ ISkMInstanceManager interface forwarding
	FText GetSkMInstanceDisplayName() const { return InstanceManager->GetSkMInstanceDisplayName(InstanceId); }
	FText GetSkMInstanceTooltip() const { return InstanceManager->GetSkMInstanceTooltip(InstanceId); }
	bool CanEditSkMInstance() const { return InstanceManager->CanEditSkMInstance(InstanceId); }
	bool CanMoveSkMInstance(const ETypedElementWorldType WorldType) const { return InstanceManager->CanMoveSkMInstance(InstanceId, WorldType); }
	bool GetSkMInstanceTransform(FTransform& OutInstanceTransform, bool bWorldSpace = false) const { return InstanceManager->GetSkMInstanceTransform(InstanceId, OutInstanceTransform, bWorldSpace); }
	bool SetSkMInstanceTransform(const FTransform& InstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) { return InstanceManager->SetSkMInstanceTransform(InstanceId, InstanceTransform, bWorldSpace, bMarkRenderStateDirty, bTeleport); }
	void NotifySkMInstanceMovementStarted() { InstanceManager->NotifySkMInstanceMovementStarted(InstanceId); }
	void NotifySkMInstanceMovementOngoing() { InstanceManager->NotifySkMInstanceMovementOngoing(InstanceId); }
	void NotifySkMInstanceMovementEnded() { InstanceManager->NotifySkMInstanceMovementEnded(InstanceId); }
	void NotifySkMInstanceSelectionChanged(const bool bIsSelected) { InstanceManager->NotifySkMInstanceSelectionChanged(InstanceId, bIsSelected); }
	void ForEachSkMInstanceInSelectionGroup(TFunctionRef<bool(FSkMInstanceId)> Callback) { InstanceManager->ForEachSkMInstanceInSelectionGroup(InstanceId, Callback); }
	bool CanDeleteSkMInstance() const { return InstanceManager->CanDeleteSkMInstance(InstanceId); }
	bool DeleteSkMInstance() { return InstanceManager->DeleteSkMInstances(MakeArrayView(&InstanceId, 1)); }
	bool CanDuplicateSkMInstance() const { return InstanceManager->CanDuplicateSkMInstance(InstanceId); }
	bool DuplicateSkMInstance(FSkMInstanceId& OutNewInstanceId)
	{
		TArray<FSkMInstanceId> NewInstanceIds;
		if (InstanceManager->DuplicateSkMInstances(MakeArrayView(&InstanceId, 1), NewInstanceIds))
		{
			OutNewInstanceId = NewInstanceIds[0];
			return true;
		}
		return false;
	}

private:
	FSkMInstanceId InstanceId;
	ISkMInstanceManager* InstanceManager = nullptr;
};

/**
 * Abstract proxy UObject for representing a skinned mesh instance in the details panel.
 * Subclass this to provide custom per-instance property editing.
 */
UCLASS(MinimalAPI, Abstract)
class USkMInstanceProxyEditingObject : public UObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(const FSkMInstanceElementId& InSkMInstanceElementId) {}
	virtual void Shutdown() {}

private:
	virtual bool SyncProxyStateFromInstance() { return false; }
};
