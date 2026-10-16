// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSubsystemBase.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"

ARCMASS_API DECLARE_LOG_CATEGORY_EXTERN(LogArcMassSpawnDiag, Display, All);

#include "ArcMassSpawnerSubsystem.generated.h"

class AActor;
class UArcMassEntityConfigAsset;
class UMassAgentComponent;

/**
 * World subsystem that owns the ArcMass entity spawn pipeline.
 *
 * Consumes UArcMassEntityConfigAsset, runs the engine trait pipeline for
 * regular fragment/tag/shared composition via FMassEntityTemplate, then
 * layers project-declared sparse fragments (via UArcMassEntityTraitBase::BuildSparseFragments)
 * and post-Commit initializers on top via FEntityBuilder.
 *
 * All public API is C++-only — no UFUNCTION; spawners are not called from BP today.
 */
UCLASS()
class ARCMASS_API UArcMassSpawnerSubsystem : public UMassSubsystemBase
{
	GENERATED_BODY()

public:

	/** Spawn a single entity from Cfg at Xform. Returns invalid handle on error. */
	FMassEntityHandle SpawnEntity(UArcMassEntityConfigAsset* Cfg, const FTransform& Xform);

	/**
	 * Spawn Count entities from Cfg, all at Xform. Results appended to OutHandles.
	 *
	 * Uses the engine batch path (BatchCreateEntities + BatchSetEntityFragmentValues) for
	 * efficient N-entity creation; sparse fragments seeded per-entity after batch creation.
	 *
	 * Returns the FEntityCreationContext that postpones OnEntitiesCreated observer notifications
	 * until the caller releases the shared pointer. Callers that don't care can discard the return.
	 * Returns nullptr on error (null Cfg, Count <= 0, invalid EntityManager, Actor-bound config).
	 */
	TSharedPtr<FMassEntityManager::FEntityCreationContext>
	SpawnEntities(UArcMassEntityConfigAsset* Cfg, int32 Count, const FTransform& Xform, TArray<FMassEntityHandle>& OutHandles);

	/**
	 * Spawn a single entity with per-call fragment overrides (regular or sparse).
	 * Overrides are applied after trait-emitted and asset-level sparse fragments —
	 * last write on type collision wins (FEntityBuilder::Add replace-on-duplicate semantics).
	 */
	FMassEntityHandle SpawnEntityWithOverrides(UArcMassEntityConfigAsset* Cfg, const FTransform& Xform, TConstArrayView<FInstancedStruct> Overrides);

	/**
	 * Spawn an entity for the given UMassAgentComponent, using its EntityConfig.Parent as the
	 * UArcMassEntityConfigAsset.
	 *
	 * Drives the UMassAgentComponent state machine: transitions None -> EntityPendingCreation before
	 * the spawn, and EntityPendingCreation -> None via EntityCreationAborted() if the spawn fails.
	 * The CALLER is responsible for calling SetEntityHandle(Handle) on success to transition to
	 * EntityCreated (this is intentional — caller may need to stamp project-side bookkeeping between
	 * spawn and SetEntityHandle).
	 *
	 * Returns an invalid handle and logs a warning if:
	 *   - World is not a game world.
	 *   - EntityConfig.Parent is null or not a UArcMassEntityConfigAsset.
	 *   - The spawner subsystem isn't available.
	 *   - The spawned entity isn't built synchronously (would happen only if called mid-tick while
	 *     IsProcessing() is true; FEntityBuilder.Commit auto-defers in that case).
	 */
	static FMassEntityHandle SpawnForAgentComponent(UMassAgentComponent& AgentComp);

	/**
	 * Counterpart to SpawnForAgentComponent. Destroys the entity (via deferred command) and clears
	 * the AgentComponent's entity handle (EntityCreated -> None).
	 * No-op if Handle is invalid or world is gone.
	 */
	static void DestroySpawnedForAgentComponent(UMassAgentComponent& AgentComp, FMassEntityHandle Handle);

	/**
	 * Bulk-destroy a set of entities. Defers per-handle when called mid-processing
	 * (Mass forbids synchronous destroy during Execute), otherwise routes through
	 * BatchDestroyEntityChunks for the per-archetype fast path. No-op on empty input
	 * or invalid EntityManager.
	 */
	void DestroyEntities(TConstArrayView<FMassEntityHandle> Handles);

protected:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	TSharedPtr<FMassEntityManager> EntityManager;

	/**
	 * Core per-entity spawn logic shared by all public entry points.
	 * EntityManager must be valid (caller is responsible for the null check).
	 *
	 * OwnerForInitializers: AActor supplied by AgentComp-bound callers. If non-null, engine
	 * ObjectFragmentInitializerFunction callbacks declared on the template (e.g. by
	 * UMassAgentCapsuleCollisionSyncTrait, UMoverMassAgentTrait, UMassLookAtTargetTrait) are
	 * run post-Commit with this Owner.
	 */
	FMassEntityHandle BuildOne(
		TSharedRef<FMassEntityManager> InEntityManager,
		UArcMassEntityConfigAsset& Cfg,
		const FTransform& Xform,
		TConstArrayView<FInstancedStruct> Overrides,
		AActor* OwnerForInitializers = nullptr);

	/**
	 * Seeds a sparse fragment on an existing entity. If the fragment is already present, overwrites
	 * its value. If not present, adds it (default-constructed via AddSparseElementToEntity) then
	 * copies the value. No-op if Value has no type or the entity is not built.
	 */
	void SeedSparse(FMassEntityHandle Handle, const FInstancedStruct& Value);
};
