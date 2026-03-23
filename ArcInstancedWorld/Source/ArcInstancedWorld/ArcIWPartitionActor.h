// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"
#include "Engine/ActorInstanceManagerInterface.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "ArcIWTypes.h"
#include "MassEntityHandle.h"
#include "ArcIWPartitionActor.generated.h"

class UMassEntityConfigAsset;
class UInstancedStaticMeshComponent;
class UArcIWISMComponent;
class UMaterialInterface;
struct FArcMassPhysicsLinkFragment;

/**
 * Partition actor that stores actor class data and spawns/despawns Mass entities
 * when cells are loaded/unloaded by World Partition.
 * Implements IActorInstanceManagerInterface for automatic trace hydration.
 */
UCLASS()
class ARCINSTANCEDWORLD_API AArcIWPartitionActor : public APartitionActor, public IActorInstanceManagerInterface
{
	GENERATED_BODY()

public:
	AArcIWPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Look up grid cell size from the WP runtime hash by grid name.
	 *  Falls back to UArcIWSettings::DefaultGridCellSize. Static so callers don't need an instance. */
	static uint32 GetGridCellSize(UWorld* InWorld, FName InGridName);

#if WITH_EDITOR
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override { return GetGridCellSize(InWorld, GridName); }
#endif

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostRegisterAllComponents() override;

#if WITH_EDITOR
	/**
	 * Adds an actor instance to this partition cell.
	 * Finds or creates an entry by actor class and appends the transform.
	 */
	void AddActorInstance(
		TSubclassOf<AActor> ActorClass,
		const FTransform& WorldTransform,
		const TArray<FArcIWMeshEntry>& MeshDescriptors,
		UMassEntityConfigAsset* InAdditionalConfig);

	/** Set the grid name. Called during editor conversion. */
	void SetGridName(FName InGridName) { GridName = InGridName; }

	/** Create editor-only ISM components to preview instances in the level editor.
	 *  These are stripped in PIE/game worlds. */
	void RebuildEditorPreviewISMCs();
	void DestroyEditorPreviewISMCs();
#endif

	/** Returns the stored per-class data entries. */
	const TArray<FArcIWActorClassData>& GetActorClassEntries() const { return ActorClassEntries; }

	FName GetGridName() const { return GridName; }

	// --- ISM Management ---

	/** Add ISM instances for a composite entity. Uses MeshSlotBase for direct array indexing. */
	void AddCompositeISMInstances(
		int32 MeshSlotBase,
		const FTransform& WorldTransform,
		const TArray<FArcIWMeshEntry>& MeshDescriptors,
		TArray<int32>& InOutISMInstanceIds,
		int32 InInstanceIndex);

	/** Remove ISM instances with swap-fixup so swapped entity's ISMInstanceIds stay correct. */
	void RemoveCompositeISMInstances(
		int32 MeshSlotBase,
		const TArray<FArcIWMeshEntry>& MeshDescriptors,
		const TArray<int32>& ISMInstanceIds,
		FMassEntityManager& EntityManager);

	/** Populate physics link fragment entries from ISM instance IDs after AddCompositeISMInstances. */
	void PopulatePhysicsLinkEntries(
		FArcMassPhysicsLinkFragment& LinkFragment,
		int32 MeshSlotBase,
		const TArray<int32>& ISMInstanceIds);

	/** Detach all physics link entries and clear. Safe to call with empty fragment. */
	static void DetachPhysicsLinkEntries(FArcMassPhysicsLinkFragment& LinkFragment);

	/** Resolve ISM component + instance index to the owning Mass entity. */
	FMassEntityHandle ResolveISMInstanceToEntity(const UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex) const;

	/** Resolve ISM instance to entity + flat instance index. */
	bool ResolveISMInstanceToOwner(const UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex,
		FMassEntityHandle& OutEntity, int32& OutInstanceIndex) const;

	/** Hydrate a specific entity — remove ISM, spawn actor via pool. No-op if already hydrated. */
	void HydrateEntity(FMassEntityHandle EntityHandle);

	//~ IActorInstanceManagerInterface
	virtual int32 ConvertCollisionIndexToInstanceIndex(int32 InIndex, const UPrimitiveComponent* RelevantComponent) const override;
	virtual AActor* FindActor(const FActorInstanceHandle& Handle) override;
	virtual AActor* FindOrCreateActor(const FActorInstanceHandle& Handle) override;
	virtual UClass* GetRepresentedClass(int32 InstanceIndex) const override;
	virtual ULevel* GetLevelForInstance(int32 InstanceIndex) const override;
	virtual FTransform GetTransform(const FActorInstanceHandle& Handle) const override;
	//~

private:
	void SpawnEntities();
	void DespawnEntities();

	/** World Partition runtime grid name this partition belongs to. */
	UPROPERTY(VisibleAnywhere, Category = "ArcIW")
	FName GridName = TEXT("MainGrid");

	/** Per-actor-class serialized data. */
	UPROPERTY(VisibleAnywhere, Category = "ArcIW")
	TArray<FArcIWActorClassData> ActorClassEntries;

	/** Flat array of all spawned entity handles. The array index is the instance index
	 *  used by IActorInstanceManagerInterface. */
	TArray<FMassEntityHandle> SpawnedEntities;

	/** Pre-allocated ISM components — one per mesh descriptor entry across all classes.
	 *  Layout: [Class0.Mesh0, Class0.Mesh1, ..., Class1.Mesh0, ...].
	 *  Indexed by FArcIWInstanceFragment::MeshSlotBase + MeshDescriptorIndex.
	 *  Each component stores its own MeshSlotIndex and Owners array. */
	TArray<TObjectPtr<UArcIWISMComponent>> MeshSlots;

#if WITH_EDITORONLY_DATA
	/** Editor-only ISM components for previewing instances. Stripped in PIE/game. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> EditorPreviewISMCs;
#endif
};
