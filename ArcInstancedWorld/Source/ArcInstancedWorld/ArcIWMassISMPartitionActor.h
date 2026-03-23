// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"
#include "Engine/ActorInstanceManagerInterface.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "ArcIWTypes.h"
#include "MassEntityHandle.h"
#include "ArcIWMassISMPartitionActor.generated.h"

class UMassEntityConfigAsset;
class UInstancedStaticMeshComponent;

/**
 * Partition actor that uses Mass MeshEngine for ISM rendering and standalone
 * FBodyInstance for physics (no ISM components at runtime).
 * Implements IActorInstanceManagerInterface for automatic trace hydration.
 */
UCLASS()
class ARCINSTANCEDWORLD_API AArcIWMassISMPartitionActor : public APartitionActor, public IActorInstanceManagerInterface
{
	GENERATED_BODY()

public:
	AArcIWMassISMPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Look up grid cell size from the WP runtime hash by grid name.
	 *  Falls back to UArcIWSettings::DefaultGridCellSize. Uses AArcIWPartitionActor's static helper. */
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

	/** Hydrate a specific entity -- terminate physics bodies, spawn actor via pool. No-op if already hydrated. */
	void HydrateEntity(FMassEntityHandle EntityHandle);

	/** Activate entity — add ISM instances to holders + create physics bodies (async).
	 *  Called by UArcIWActivateProcessor when entity enters swap radius. */
	void ActivateEntity(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWVisConfigFragment& Config,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager);

	/** Deactivate entity — remove ISM instances from holders + destroy physics bodies.
	 *  Called by UArcIWDeactivateProcessor when entity exits swap radius. */
	void DeactivateEntity(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWVisConfigFragment& Config,
		FMassEntityManager& EntityManager);

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
	void CreateISMHolderEntities();
	void DestroyISMHolderEntities();

	/** World Partition runtime grid name this partition belongs to. */
	UPROPERTY(VisibleAnywhere, Category = "ArcIW")
	FName GridName = TEXT("MainGrid");

	/** Per-actor-class serialized data. */
	UPROPERTY(VisibleAnywhere, Category = "ArcIW")
	TArray<FArcIWActorClassData> ActorClassEntries;

	/** Flat array of all spawned entity handles. The array index is the instance index
	 *  used by IActorInstanceManagerInterface. */
	TArray<FMassEntityHandle> SpawnedEntities;

	/** Mass MeshEngine ISM holder entities -- one per mesh slot across all classes.
	 *  Layout: [Class0.Mesh0, Class0.Mesh1, ..., Class1.Mesh0, ...].
	 *  Each holds FMassRenderStateFragment + FMassRenderISMFragment for rendering. */
	TArray<FMassEntityHandle> ISMHolderEntities;

#if WITH_EDITORONLY_DATA
	/** Editor-only ISM components for previewing instances. Stripped in PIE/game. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> EditorPreviewISMCs;
#endif
};
