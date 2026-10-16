// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "DataStorage/Handles.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"

#include "ArcMassOutlinerRowManager.generated.h"

class ULevel;

/**
 * Per-actor bookkeeping for synthetic TEDS outliner rows.
 *
 * ConfigGroupRows maps EntryIndex -> TEDS RowHandle for the config-group row.
 * InstanceRows maps (EntryIndex, TransformIndex) -> TEDS RowHandle for each instance row.
 * ChangedHandle is the FDelegateHandle returned by Actor->OnEditorStateChanged.AddUObject(…).
 */
struct FArcOutlinerActorState
{
	TMap<int32, UE::Editor::DataStorage::RowHandle> ConfigGroupRows;
	TMap<TPair<int32, int32>, UE::Editor::DataStorage::RowHandle> InstanceRows;
	FDelegateHandle ChangedHandle;
};

/**
 * UArcMassOutlinerRowManager
 *
 * UEditorSubsystem that owns the TEDS outliner rows for every
 * AArcPlacedEntityPartitionActor in the editor world.  It maintains a
 * two-level tree (config-group rows → instance rows) parented under the
 * actor's existing TEDS row, reacting to:
 *   - AArcPlacedEntityPartitionActor::OnEditorStateChanged (Rebuilt / TornDown / Destroyed)
 *   - GEngine->OnLevelActorAdded / OnLevelActorDeleted
 */
UCLASS()
class UArcMassOutlinerRowManager : public UEditorSubsystem
{
	GENERATED_BODY()

public:

	// ~UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End UEditorSubsystem interface

private:

	/** Called once TEDS features are available. Scans the editor world and sets up engine delegates. */
	void OnTedsReady();

	/** Iterate the current editor world and (re)build rows for every partition actor found. */
	void SeedActorsInEditorWorld();

	/** FEditorDelegates::OnMapOpened handler — re-seeds rows after a map (esp. a WP map) is opened. */
	void OnMapOpened(const FString& Filename, bool bAsTemplate);

	/**
	 * (Re)bind ULevel::OnLoadedActorAddedToLevelEvent / OnLoadedActorRemovedFromLevelEvent on the current
	 * editor world's persistent level. WP streams partition actors in/out via AddLoadedActor (bypassing
	 * UWorld::SpawnActor, so GEngine->OnLevelActorAdded never fires for them) — these per-level events are
	 * the canonical editor-time signal. Unbinds the previously-bound level first; safe to call repeatedly.
	 */
	void BindLevelLoadedActorEvents();

	/** ULevel::OnLoadedActorAddedToLevelEvent handler — builds rows when a partition actor streams in. */
	void OnLoadedActorAddedToLevel(AActor& InActor);

	/** ULevel::OnLoadedActorRemovedFromLevelEvent handler — tears down rows when a partition actor streams out. */
	void OnLoadedActorRemovedFromLevel(AActor& InActor);

	/**
	 * Tear down existing rows for Actor and rebuild them fresh from Actor->GetEntries().
	 * Binds Actor->OnEditorStateChanged if not already bound.
	 */
	void RebuildActorRows(AArcPlacedEntityPartitionActor* Actor);

	/**
	 * Remove all TEDS rows owned by this actor from the storage.
	 * Does NOT remove the state map entry or unbind the delegate; callers that are fully
	 * removing the actor do that themselves.
	 */
	void TearDownActorRows(AArcPlacedEntityPartitionActor* Actor);

	/** Delegate callback from AArcPlacedEntityPartitionActor::OnEditorStateChanged. */
	void OnActorStateChanged(AArcPlacedEntityPartitionActor* Actor, EArcPlacedEntityOutlinerEvent Event);

	// -------------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------------

	/** Per-actor bookkeeping. Key is weak to allow GC without manual cleanup. */
	TMap<TWeakObjectPtr<AArcPlacedEntityPartitionActor>, FArcOutlinerActorState> ActorStates;

	/** Handle for the TEDS "features enabled" one-shot callback (bound only when TEDS was not yet ready at Initialize time). */
	FDelegateHandle FeaturesEnabledHandle;

	/** Handle for GEngine->OnLevelActorAdded. */
	FDelegateHandle LevelActorAddedHandle;

	/** Handle for GEngine->OnLevelActorDeleted. */
	FDelegateHandle LevelActorDeletedHandle;

	/** Handle for FEditorDelegates::OnMapOpened. */
	FDelegateHandle MapOpenedHandle;

	/** The persistent level we currently have OnLoadedActor*ToLevelEvent bound on (may change on map open). */
	TWeakObjectPtr<ULevel> BoundLevel;

	/** Handle for BoundLevel->OnLoadedActorAddedToLevelEvent. */
	FDelegateHandle LoadedActorAddedHandle;

	/** Handle for BoundLevel->OnLoadedActorRemovedFromLevelEvent. */
	FDelegateHandle LoadedActorRemovedHandle;
};
