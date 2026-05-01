// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Elements/SMInstance/SMInstanceManager.h"
#include "StructUtils/InstancedStruct.h"
#include "Containers/Ticker.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementId.h"
#include "ArcPlacedEntityProxyEditingObject.generated.h"

class AArcPlacedEntityPartitionActor;
class UInstancedSkinnedMeshComponent;

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
	virtual void Initialize(const FSMInstanceElementId& InSMInstanceElementId) override;
	virtual void Shutdown() override;

	/** Initialize from a skinned mesh instance element ID (SkMInstance typed element path). */
	void InitFromSkMInstance(const FSkMInstanceElementId& InSkMInstanceElementId);

	/** Returns true if this proxy was created from a SkMInstance (skinned mesh) element. */
	bool IsSkMInstanceProxy() const { return bIsSkMInstance; }

	virtual void BeginDestroy() override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

	UPROPERTY(EditAnywhere, Category = "Transform", meta = (ShowOnlyInnerProperties))
	FTransform Transform;

	/** Fragment overrides currently being edited. Populated in Initialize(). */
	TArray<FInstancedStruct> EditableFragments;

	/** Display names for each editable fragment (parallel to EditableFragments). */
	TArray<FString> FragmentDisplayNames;

	bool IsValid() const { return PartitionActor.IsValid() && EntryIndex != INDEX_NONE; }

	int32 GetEntryIndex() const { return EntryIndex; }
	int32 GetTransformIndex() const { return TransformIndex; }
	AArcPlacedEntityPartitionActor* GetPartitionActor() const { return PartitionActor.Get(); }

	/** Write current EditableFragments back to the partition actor's override map. */
	void WriteOverridesToPartitionActor();

private:
	virtual bool SyncProxyStateFromInstance() override;

	FSMInstanceManager GetSMInstance() const;

	bool ResolveToPartitionActor(const FSMInstanceElementId& InSMInstanceElementId);
	bool ResolveToPartitionActorFromSkM(const FSkMInstanceElementId& InSkMInstanceElementId);
	void BuildEditableFragmentList();

	/** Extract ticker setup so both Initialize and InitFromSkMInstance can share it. */
	void StartSyncTicker();

	TWeakObjectPtr<UInstancedStaticMeshComponent> ISMComponent;
	uint64 ISMInstanceId = 0;

	// SkMInstance path members
	TWeakObjectPtr<UInstancedSkinnedMeshComponent> SkMComponent;
	int32 SkMPrimitiveInstanceId = INDEX_NONE;
	bool bIsSkMInstance = false;

	TWeakObjectPtr<AArcPlacedEntityPartitionActor> PartitionActor;
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;

	FTSTicker::FDelegateHandle TickHandle;
	bool bIsWithinInteractiveTransformEdit = false;
};
