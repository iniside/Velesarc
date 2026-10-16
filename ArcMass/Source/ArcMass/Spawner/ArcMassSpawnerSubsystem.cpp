// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSpawnerSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityBuilder.h"
#include "MassEntityTemplate.h"
#include "MassEntityConfigAsset.h"
#include "Mass/EntityView.h"
#include "MassAgentComponent.h"
#include "Mass/EntityFragments.h"
#include "MassSimulationSubsystem.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"
#include "HAL/LowLevelMemTracker.h"

#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "ArcMass/Config/ArcMassEntityInitializer.h"
#include "ArcMass/MobileVisualization/ArcMobileVisualization.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassSpawner, Warning, All);
DEFINE_LOG_CATEGORY(LogArcMassSpawnDiag);

//-----------------------------------------------------------------------------
// Lifecycle
//-----------------------------------------------------------------------------

void UArcMassSpawnerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassSimulationSubsystem>();
	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World).AsShared();
	}
}

void UArcMassSpawnerSubsystem::Deinitialize()
{
	EntityManager.Reset();
	Super::Deinitialize();
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------

FMassEntityHandle UArcMassSpawnerSubsystem::SpawnEntity(UArcMassEntityConfigAsset* Cfg, const FTransform& Xform)
{
	LLM_SCOPE_BYNAME(TEXT("Mass/Spawner"));
	if (Cfg == nullptr)
	{
		UE_LOGF(LogTemp, Warning,"UArcMassSpawnerSubsystem::SpawnEntity: Cfg is null — cannot spawn entity.");
		return FMassEntityHandle();
	}

	if (!EntityManager.IsValid())
	{
		return FMassEntityHandle();
	}

	return BuildOne(EntityManager.ToSharedRef(), *Cfg, Xform, TConstArrayView<FInstancedStruct>());
}

TSharedPtr<FMassEntityManager::FEntityCreationContext>
UArcMassSpawnerSubsystem::SpawnEntities(UArcMassEntityConfigAsset* Cfg, int32 Count,
                                         const FTransform& Xform, TArray<FMassEntityHandle>& OutHandles)
{
	LLM_SCOPE_BYNAME(TEXT("Mass/Spawner"));
	if (Cfg == nullptr || Count <= 0 || !EntityManager.IsValid())
	{
		return TSharedPtr<FMassEntityManager::FEntityCreationContext>();
	}

	const FMassEntityTemplate& Template = Cfg->GetConfig().GetOrCreateEntityTemplate(*GetWorld());

	// Reject Actor-bound configs unless mobile-vis, which runs ObjectFragmentInitializers when it spawns the actor.
	if (Template.GetObjectFragmentInitializers().Num() > 0
		&& !Template.GetCompositionDescriptor().Contains<FArcMobileVisEntityTag>())
	{
		UE_LOGF(LogArcMassSpawner, Error,"SpawnEntities: config '%ls' carries ObjectInitializers; use SpawnForAgentComponent.",
			*Cfg->GetName());
		return TSharedPtr<FMassEntityManager::FEntityCreationContext>();
	}

	// 1. Walk traits — gather sparse contributions (same across all N entities).
	FArcSparseFragmentBuildContext SparseCtx;
	TArray<const UArcMassEntityTraitBase*, TInlineAllocator<8>> InitTraits;
	TArray<UMassEntityTraitBase*> Traits;
	ArcMass::CollectCombinedTraits(Cfg->GetConfig(), GetWorld(), Traits);
	for (UMassEntityTraitBase* T : Traits)
	{
		const UArcMassEntityTraitBase* ArcTrait = Cast<const UArcMassEntityTraitBase>(T);
		if (ArcTrait != nullptr)
		{
			ArcTrait->BuildSparseFragments(SparseCtx, *GetWorld());
			InitTraits.Add(ArcTrait);
		}
	}

	// 2. Build regular-fragment instance list: template initial values + transform stamp.
	//    Sparse fragments are handled per-entity in step 5.
	TArray<FInstancedStruct, TInlineAllocator<16>> RegularValues;
	RegularValues.Append(Template.GetInitialFragmentValues());
	{
		FTransformFragment TF;
		TF.SetTransform(Xform);
		RegularValues.Add(FInstancedStruct::Make<FTransformFragment>(TF));
	}

	// 3. Batch-create — one archetype allocation for all Count entities.
	TArray<FMassEntityHandle> Created;
	Created.Reserve(Count);
	TSharedRef<FMassEntityManager::FEntityCreationContext> CreationCtx =
		EntityManager->BatchCreateEntities(
			Template.GetArchetype(),
			Template.GetSharedFragmentValues(),
			Count,
			Created);

	// 4. Batch-write regular fragment values to all created entities.
	TArray<FMassArchetypeEntityCollection> Collections;
	UE::Mass::Utils::CreateEntityCollections(
		*EntityManager,
		TConstArrayView<FMassEntityHandle>(Created),
		FMassArchetypeEntityCollection::NoDuplicates,
		Collections);
	EntityManager->BatchSetEntityFragmentValues(
		TConstArrayView<FMassArchetypeEntityCollection>(Collections),
		TArrayView<const FInstancedStruct>(RegularValues));

	// 5. Per-entity sparse seeding + post-Commit init.
	//    Loops over N entities, but body is cheap vs a full per-entity FEntityBuilder::Commit.
	for (const FMassEntityHandle& Handle : Created)
	{
		for (const FInstancedStruct& S : SparseCtx.SparseFragments)
		{
			SeedSparse(Handle, S);
		}
		for (const FInstancedStruct& S : Cfg->InitialSparseFragments)
		{
			SeedSparse(Handle, S);
		}
		for (const UScriptStruct* TagType : SparseCtx.SparseTagTypes)
		{
			if (TagType)
			{
				EntityManager->AddTagToEntity(Handle, TagType);
			}
		}
		for (const UArcMassEntityTraitBase* ArcTrait : InitTraits)
		{
			ArcTrait->InitializeEntity(*EntityManager, Handle, *GetWorld());
		}
		for (TObjectPtr<UArcMassEntityInitializer> Init : Cfg->PostCommitInitializers)
		{
			if (Init != nullptr)
			{
				Init->InitializeEntity(*EntityManager, Handle, *Cfg);
			}
		}
	}

	OutHandles.Append(MoveTemp(Created));

	// Return the creation context — caller may hold it to postpone OnEntitiesCreated observers.
	// Releasing (or discarding) the returned pointer fires observer notifications.
	return CreationCtx.ToSharedPtr();
}

//-----------------------------------------------------------------------------
// Private helpers
//-----------------------------------------------------------------------------

void UArcMassSpawnerSubsystem::SeedSparse(FMassEntityHandle Handle, const FInstancedStruct& Value)
{
	const UScriptStruct* Type = Value.GetScriptStruct();
	if (Type == nullptr || !EntityManager->IsEntityBuilt(Handle))
	{
		return;
	}

	FStructView Existing = EntityManager->GetMutableSparseElementDataForEntity(Type, Handle);
	if (Existing.IsValid())
	{
		Type->CopyScriptStruct(Existing.GetMemory(), Value.GetMemory());
	}
	else
	{
		FStructView Added = EntityManager->AddSparseElementToEntity(Handle, Type);
		if (Added.IsValid())
		{
			Type->CopyScriptStruct(Added.GetMemory(), Value.GetMemory());
		}
	}
}

FMassEntityHandle UArcMassSpawnerSubsystem::SpawnEntityWithOverrides(UArcMassEntityConfigAsset* Cfg, const FTransform& Xform, TConstArrayView<FInstancedStruct> Overrides)
{
	LLM_SCOPE_BYNAME(TEXT("Mass/Spawner"));
	if (Cfg == nullptr)
	{
		UE_LOGF(LogTemp, Warning,"UArcMassSpawnerSubsystem::SpawnEntityWithOverrides: Cfg is null — cannot spawn entity.");
		return FMassEntityHandle();
	}

	if (!EntityManager.IsValid())
	{
		return FMassEntityHandle();
	}

	return BuildOne(EntityManager.ToSharedRef(), *Cfg, Xform, Overrides);
}

void UArcMassSpawnerSubsystem::DestroyEntities(TConstArrayView<FMassEntityHandle> Handles)
{
	LLM_SCOPE_BYNAME(TEXT("Mass/Spawner"));
	if (!EntityManager.IsValid() || Handles.Num() == 0)
	{
		return;
	}

	// Mass forbids synchronous destroy mid-Execute — defer per handle in that case.
	if (EntityManager->IsProcessing())
	{
		for (const FMassEntityHandle& H : Handles)
		{
			if (H.IsValid())
			{
				EntityManager->Defer().DestroyEntity(H);
			}
		}
		return;
	}

	TArray<FMassArchetypeEntityCollection> Collections;
	UE::Mass::Utils::CreateEntityCollections(
		*EntityManager,
		Handles,
		FMassArchetypeEntityCollection::NoDuplicates,
		Collections);
	EntityManager->BatchDestroyEntityChunks(
		TConstArrayView<FMassArchetypeEntityCollection>(Collections));
}

//-----------------------------------------------------------------------------
// Private — core spawn logic
//-----------------------------------------------------------------------------

FMassEntityHandle UArcMassSpawnerSubsystem::BuildOne(
	TSharedRef<FMassEntityManager> InEntityManager,
	UArcMassEntityConfigAsset& Cfg,
	const FTransform& Xform,
	TConstArrayView<FInstancedStruct> Overrides,
	AActor* OwnerForInitializers)
{
	// 1. Engine template build — regular fragments, tags, const-shared + shared. Engine-cached.
	const FMassEntityTemplate& Template = Cfg.GetConfig().GetOrCreateEntityTemplate(*GetWorld());

	// 1a. Contract: configs with engine ObjectInitializers are Actor-bound. Without an Owner we
	// cannot bootstrap Actor->Entity fragments (e.g. FAgentRadiusFragment from UCapsuleComponent,
	// Mover wiring) — the entity would be silently miswired.
	// Exception: mobile-vis entities, whose initializers ArcMobileVis runs when it spawns the actor.
	TConstArrayView<FMassEntityTemplateData::FObjectFragmentInitializerFunction> Initializers
		= Template.GetObjectFragmentInitializers();
	if (Initializers.Num() > 0 && OwnerForInitializers == nullptr
		&& !Template.GetCompositionDescriptor().Contains<FArcMobileVisEntityTag>())
	{
		UE_LOGF(LogArcMassSpawner, Error,"BuildOne: config '%ls' carries %d ObjectInitializers but no Owner was supplied. "
			     "Actor-bound configs must be spawned via SpawnForAgentComponent. Aborting spawn.",
			*Cfg.GetName(), Initializers.Num());
		return FMassEntityHandle();
	}

	// 2. Builder constructed manually — mirrors FMassEntityTemplateData::CreateEntityBuilder
	// (MassEntityTemplate.cpp:63), but skips the helper's blanket ensure that fires whenever
	// the template carries ObjectInitializers. The ensure exists because the EntityBuilder
	// pipeline drops those initializers, and the engine helper can't know whether the caller
	// has a plan.
	const FMassArchetypeSharedFragmentValues& SharedValues = Template.GetSharedFragmentValues();
	UE::Mass::FEntityBuilder Builder = UE::Mass::FEntityBuilder::Make(
		InEntityManager,
		Template.GetCompositionDescriptor(),
		Template.GetInitialFragmentValues(),
		SharedValues.GetConstSharedFragments(),
		SharedValues.GetSharedFragments());

	// GetArchetypeCreationParams is declared non-const on FMassEntityTemplateData; const_cast is
	// localised here (read-only use, we don't mutate the params).
	Builder.ConfigureArchetypeCreation(
		const_cast<FMassEntityTemplateData&>(Template.GetTemplateData()).GetArchetypeCreationParams());

	// 3. Walk traits — collect sparse contributions from UArcMassEntityTraitBase subclasses
	//    and remember which traits override InitializeEntity.
	FArcSparseFragmentBuildContext SparseCtx;
	TArray<const UArcMassEntityTraitBase*, TInlineAllocator<8>> InitTraits;

	TArray<UMassEntityTraitBase*> Traits;
	ArcMass::CollectCombinedTraits(Cfg.GetConfig(), GetWorld(), Traits);
	for (UMassEntityTraitBase* T : Traits)
	{
		const UArcMassEntityTraitBase* ArcTrait = Cast<const UArcMassEntityTraitBase>(T);
		if (ArcTrait != nullptr)
		{
			ArcTrait->BuildSparseFragments(SparseCtx, *GetWorld());
			InitTraits.Add(ArcTrait);
		}
	}

	// Checkpoint A3: log Arc trait count vs total trait count.
	UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] BuildOne: ArcTraitsFound=%d TotalTraits=%d Cfg=%ls",
		InitTraits.Num(), Traits.Num(), *Cfg.GetName());

	// 4. Apply trait-emitted sparse fragments via runtime-typed Add (replaces on duplicate).
	for (const FInstancedStruct& S : SparseCtx.SparseFragments)
	{
		Builder.Add(S);
	}

	// Checkpoint A4: log each sparse fragment type seeded by traits.
	for (const FInstancedStruct& S : SparseCtx.SparseFragments)
	{
		UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] BuildOne: SparseFragment type=%ls",
			S.GetScriptStruct() ? *S.GetScriptStruct()->GetName() : TEXT("<null>"));
	}

	// 4b. Apply trait-emitted sparse tag types via the type-only Add overload.
	for (const UScriptStruct* TagType : SparseCtx.SparseTagTypes)
	{
		if (TagType)
		{
			Builder.Add(TagType);
		}
	}

	// Checkpoint A4b: log each sparse tag type seeded by traits.
	for (const UScriptStruct* TagType : SparseCtx.SparseTagTypes)
	{
		UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] BuildOne: SparseTagType=%ls",
			TagType ? *TagType->GetName() : TEXT("<null>"));
	}

	// 5. Apply asset-level sparse overrides — replace trait-emitted defaults on type collision.
	for (const FInstancedStruct& S : Cfg.InitialSparseFragments)
	{
		Builder.Add(S);
	}

	// 6. Per-entity overrides (regular or sparse — type drives routing in FEntityBuilder).
	for (const FInstancedStruct& O : Overrides)
	{
		Builder.Add(O);
	}

	// 7. Stamp transform via runtime-typed Add of FTransformFragment (replaces existing).
	{
		FTransformFragment TF;
		TF.SetTransform(Xform);
		Builder.Add(FInstancedStruct::Make<FTransformFragment>(TF));
	}

	// Checkpoint A5: about to commit.
	UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] BuildOne: committing entity for Cfg=%ls Owner=%ls",
		*Cfg.GetName(),
		OwnerForInitializers ? *OwnerForInitializers->GetPathName() : TEXT("<null>"));

	// 8. Commit. FEntityBuilder manages its own creation context.
	const FMassEntityHandle Handle = Builder.Commit();
	if (!Handle.IsValid())
	{
		UE_LOGF(LogArcMassSpawner, Error,"BuildOne: Commit() returned an invalid handle for config '%ls'. Template: %ls",
			*Cfg.GetName(), *Template.DebugGetDescription(&InEntityManager.Get()));
		return Handle;
	}

	// Checkpoint A6: entity committed — log handle and IsEntityBuilt.
	UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] BuildOne: committed Idx=%d Serial=%d IsEntityBuilt=%d",
		Handle.Index, Handle.SerialNumber, (int32)InEntityManager->IsEntityBuilt(Handle));

	// 8.5. Run engine ObjectFragmentInitializers with the Owner — this is what
	// UMassAgentSubsystem::OnProcessingPhaseStarted does in the legacy path (MassAgentSubsystem.cpp:398).
	// Direction MassToActor mirrors the engine's own initial-pop direction for this hook.
	// Ownerless mobile-vis entities skip this; ArcMobileVis runs the initializers on actor spawn.
	if (OwnerForInitializers != nullptr && Initializers.Num() > 0)
	{
		FMassEntityView EntityView(*InEntityManager, Handle);
		// TFunction::operator() is non-const; the view gives us const elements, so const_cast at the
		// call site. We aren't mutating the functor — TFunction just doesn't expose a const call op.
		for (const FMassEntityTemplateData::FObjectFragmentInitializerFunction& Initializer : Initializers)
		{
			const_cast<FMassEntityTemplateData::FObjectFragmentInitializerFunction&>(Initializer)(
				*OwnerForInitializers, EntityView, EMassTranslationDirection::MassToActor);
		}
	}

	// 9. Post-Commit init. Outside creation context — safe for synchronous API calls.
	for (const UArcMassEntityTraitBase* ArcTrait : InitTraits)
	{
		// Checkpoint A7: log each trait about to receive InitializeEntity.
		UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] BuildOne: InitializeEntity Idx=%d Serial=%d Trait=%ls",
			Handle.Index, Handle.SerialNumber, *ArcTrait->GetClass()->GetName());
		ArcTrait->InitializeEntity(*InEntityManager, Handle, *GetWorld());
	}

	for (TObjectPtr<UArcMassEntityInitializer> Init : Cfg.PostCommitInitializers)
	{
		if (Init != nullptr)
		{
			Init->InitializeEntity(*InEntityManager, Handle, Cfg);
		}
	}

	return Handle;
}

//-----------------------------------------------------------------------------
// Static helpers — AgentComponent bridge
//-----------------------------------------------------------------------------

FMassEntityHandle UArcMassSpawnerSubsystem::SpawnForAgentComponent(UMassAgentComponent& AgentComp)
{
	UWorld* World = AgentComp.GetWorld();
	if (World == nullptr || !World->IsGameWorld())
	{
		return FMassEntityHandle();
	}

	// Checkpoint A1: log entry — AgentComp, Owner, EntityConfig.Parent.
	{
		const UObject* ParentObj = AgentComp.GetEntityConfig().GetParent();
		UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] SpawnForAgentComponent: AgentComp=%ls Owner=%ls ParentObj=%ls ParentClass=%ls",
			*AgentComp.GetPathName(),
			AgentComp.GetOwner() ? *AgentComp.GetOwner()->GetPathName() : TEXT("<null>"),
			ParentObj ? *ParentObj->GetPathName() : TEXT("<null>"),
			ParentObj ? *ParentObj->GetClass()->GetName() : TEXT("<null>"));
	}

	const UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(AgentComp.GetEntityConfig().GetParent());

	// Checkpoint A2: log cast result.
	if (ArcCfg == nullptr)
	{
		const UObject* ParentObj = AgentComp.GetEntityConfig().GetParent();
		if (ParentObj == nullptr)
		{
			UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] SpawnForAgentComponent: cast failed — parent is null on actor=%ls",
				AgentComp.GetOwner() ? *AgentComp.GetOwner()->GetPathName() : TEXT("<null>"));
		}
		else
		{
			UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] SpawnForAgentComponent: cast failed — wrong class=%ls on actor=%ls",
				*ParentObj->GetClass()->GetName(),
				AgentComp.GetOwner() ? *AgentComp.GetOwner()->GetPathName() : TEXT("<null>"));
		}

		UE_LOGF(LogArcMassSpawner, Warning,"UArcMassSpawnerSubsystem::SpawnForAgentComponent: EntityConfig.Parent is null or not a UArcMassEntityConfigAsset on actor '%ls'.",
			*GetNameSafe(AgentComp.GetOwner()));
		return FMassEntityHandle();
	}

	UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] SpawnForAgentComponent: cast succeeded — ArcCfg=%ls",
		*ArcCfg->GetPathName());

	UArcMassSpawnerSubsystem* Spawner = World->GetSubsystem<UArcMassSpawnerSubsystem>();
	if (!ensure(Spawner != nullptr))
	{
		return FMassEntityHandle();
	}

	if (!Spawner->EntityManager.IsValid())
	{
		return FMassEntityHandle();
	}

	AgentComp.EntityCreationPending();

	// Call BuildOne directly so we can supply the Owner for engine ObjectFragmentInitializers
	// (e.g. UMassAgentCapsuleCollisionSyncTrait, UMoverMassAgentTrait, UMassLookAtTargetTrait).
	// The public SpawnEntity path forbids initializer-bearing configs because it has no Owner.
	const FMassEntityHandle Handle = Spawner->BuildOne(
		Spawner->EntityManager.ToSharedRef(),
		*const_cast<UArcMassEntityConfigAsset*>(ArcCfg),
		AgentComp.GetOwner()->GetActorTransform(),
		TConstArrayView<FInstancedStruct>(),
		AgentComp.GetOwner());
	if (!Handle.IsValid())
	{
		AgentComp.EntityCreationAborted();
		return FMassEntityHandle();
	}

	// Risk-mitigation: if the entity was not built synchronously (e.g. called mid-tick while
	// IsProcessing() is true and FEntityBuilder::Commit auto-deferred), the handle exists but the
	// archetype is not yet materialized. Callers that stamp fragments immediately after this call
	// would corrupt state. Abort and clean up rather than propagate a half-built entity.
	FMassEntityManager& RawEntityManager = *Spawner->EntityManager.Get();
	ensureMsgf(RawEntityManager.IsEntityBuilt(Handle),
		TEXT("UArcMassSpawnerSubsystem::SpawnForAgentComponent: Entity was not built synchronously on actor '%s'. "
		     "SpawnForAgentComponent must not be called while Mass IsProcessing() is true."),
		*GetNameSafe(AgentComp.GetOwner()));

	if (!RawEntityManager.IsEntityBuilt(Handle))
	{
		AgentComp.EntityCreationAborted();
		RawEntityManager.Defer().DestroyEntity(Handle);
		return FMassEntityHandle();
	}

	return Handle;
}

void UArcMassSpawnerSubsystem::DestroySpawnedForAgentComponent(UMassAgentComponent& AgentComp, FMassEntityHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	UWorld* World = AgentComp.GetWorld();
	if (World == nullptr || !World->IsGameWorld())
	{
		return;
	}

	UArcMassSpawnerSubsystem* Spawner = World->GetSubsystem<UArcMassSpawnerSubsystem>();
	if (Spawner != nullptr && Spawner->EntityManager.IsValid())
	{
		Spawner->EntityManager->Defer().DestroyEntity(Handle);
	}

	AgentComp.ClearEntityHandle();
}
