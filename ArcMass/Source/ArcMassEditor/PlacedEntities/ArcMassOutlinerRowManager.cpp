// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEditor/PlacedEntities/ArcMassOutlinerRowManager.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "DataStorage/Features.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementHandleColumn.h"
#include "Factories/TedsEditorHierarchyFactory.h"
#include "ArcMassEditor/PlacedEntities/ArcPlacedEntityOutlinerColumns.h"
#include "ArcMassEditor/PlacedEntities/ArcPlacedEntityOutlinerFactory.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"
#include "MassEntityConfigAsset.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementId.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"

namespace ArcMassOutlinerRowManagerImpl
{

/** Collect all valid RowHandles from an actor state into OutHandles (appends). */
static void GatherRowHandles(const FArcOutlinerActorState& State, TArray<UE::Editor::DataStorage::RowHandle>& OutHandles)
{
	OutHandles.Reserve(OutHandles.Num() + State.ConfigGroupRows.Num() + State.InstanceRows.Num());
	for (const TPair<int32, UE::Editor::DataStorage::RowHandle>& Pair : State.ConfigGroupRows)
	{
		if (Pair.Value != UE::Editor::DataStorage::InvalidRowHandle)
		{
			OutHandles.Add(Pair.Value);
		}
	}
	for (const TPair<TPair<int32, int32>, UE::Editor::DataStorage::RowHandle>& Pair : State.InstanceRows)
	{
		if (Pair.Value != UE::Editor::DataStorage::InvalidRowHandle)
		{
			OutHandles.Add(Pair.Value);
		}
	}
}

} // namespace ArcMassOutlinerRowManagerImpl

// ----------------------------------------------------------------------------
// UEditorSubsystem interface
// ----------------------------------------------------------------------------

void UArcMassOutlinerRowManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UE::Editor::DataStorage::AreEditorDataStorageFeaturesEnabled())
	{
		OnTedsReady();
	}
	else
	{
		FeaturesEnabledHandle = UE::Editor::DataStorage::OnEditorDataStorageFeaturesEnabled().AddUObject(
			this, &UArcMassOutlinerRowManager::OnTedsReady);
	}
}

void UArcMassOutlinerRowManager::Deinitialize()
{
	// Unbind the one-shot features-enabled callback if it was never fired.
	if (FeaturesEnabledHandle.IsValid())
	{
		UE::Editor::DataStorage::OnEditorDataStorageFeaturesEnabled().Remove(FeaturesEnabledHandle);
		FeaturesEnabledHandle.Reset();
	}

	// Unbind the map-opened delegate.
	if (MapOpenedHandle.IsValid())
	{
		FEditorDelegates::OnMapOpened.Remove(MapOpenedHandle);
		MapOpenedHandle.Reset();
	}

	// Unbind the per-level WP loaded-actor events.
	if (ULevel* Level = BoundLevel.Get())
	{
		if (LoadedActorAddedHandle.IsValid())
		{
			Level->OnLoadedActorAddedToLevelEvent.Remove(LoadedActorAddedHandle);
		}
		if (LoadedActorRemovedHandle.IsValid())
		{
			Level->OnLoadedActorRemovedFromLevelEvent.Remove(LoadedActorRemovedHandle);
		}
	}
	LoadedActorAddedHandle.Reset();
	LoadedActorRemovedHandle.Reset();
	BoundLevel.Reset();

	// Unbind engine actor delegates.
	if (GEngine)
	{
		if (LevelActorAddedHandle.IsValid())
		{
			GEngine->OnLevelActorAdded().Remove(LevelActorAddedHandle);
			LevelActorAddedHandle.Reset();
		}
		if (LevelActorDeletedHandle.IsValid())
		{
			GEngine->OnLevelActorDeleted().Remove(LevelActorDeletedHandle);
			LevelActorDeletedHandle.Reset();
		}
	}

	// For each tracked actor: unbind its delegate and tear down its rows.
	for (TPair<TWeakObjectPtr<AArcPlacedEntityPartitionActor>, FArcOutlinerActorState>& KV : ActorStates)
	{
		AArcPlacedEntityPartitionActor* Actor = KV.Key.Get();
		if (Actor && KV.Value.ChangedHandle.IsValid())
		{
			Actor->OnEditorStateChanged.Remove(KV.Value.ChangedHandle);
			KV.Value.ChangedHandle.Reset();
		}
		TearDownActorRows(Actor ? Actor : nullptr);
	}
	ActorStates.Empty();

	Super::Deinitialize();
}

// ----------------------------------------------------------------------------
// Private implementation
// ----------------------------------------------------------------------------

void UArcMassOutlinerRowManager::OnTedsReady()
{
	// Remove the one-shot handle now that we're executing.
	if (FeaturesEnabledHandle.IsValid())
	{
		UE::Editor::DataStorage::OnEditorDataStorageFeaturesEnabled().Remove(FeaturesEnabledHandle);
		FeaturesEnabledHandle.Reset();
	}

	// Seed rows for any partition actors already in the editor world. At editor startup this is
	// usually the empty/template world (the user's WP map opens later) — OnMapOpened re-seeds then.
	SeedActorsInEditorWorld();

	// Re-seed whenever a map is opened (the WP map that actually holds the partition actors typically
	// loads well after this subsystem initializes, and WP editor actor-loading does not reliably fire
	// GEngine->OnLevelActorAdded).
	MapOpenedHandle = FEditorDelegates::OnMapOpened.AddUObject(this, &UArcMassOutlinerRowManager::OnMapOpened);

	// Bind engine-level actor lifecycle delegates (covers incremental add/remove after the map is open).
	if (GEngine)
	{
		LevelActorAddedHandle = GEngine->OnLevelActorAdded().AddLambda(
			[this](AActor* InActor)
			{
				AArcPlacedEntityPartitionActor* PartitionActor = Cast<AArcPlacedEntityPartitionActor>(InActor);
				if (PartitionActor)
				{
					RebuildActorRows(PartitionActor);
				}
			});

		LevelActorDeletedHandle = GEngine->OnLevelActorDeleted().AddLambda(
			[this](AActor* InActor)
			{
				AArcPlacedEntityPartitionActor* PartitionActor = Cast<AArcPlacedEntityPartitionActor>(InActor);
				if (!PartitionActor)
				{
					return;
				}

				TearDownActorRows(PartitionActor);

				TWeakObjectPtr<AArcPlacedEntityPartitionActor> WeakActor(PartitionActor);
				FArcOutlinerActorState* State = ActorStates.Find(WeakActor);
				if (State && State->ChangedHandle.IsValid())
				{
					PartitionActor->OnEditorStateChanged.Remove(State->ChangedHandle);
					State->ChangedHandle.Reset();
				}
				ActorStates.Remove(WeakActor);
			});
	}
}

void UArcMassOutlinerRowManager::SeedActorsInEditorWorld()
{
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (EditorWorld)
	{
		for (TActorIterator<AArcPlacedEntityPartitionActor> It(EditorWorld); It; ++It)
		{
			RebuildActorRows(*It);
		}
	}

	// Bind the per-level WP loaded-actor events so partition actors that stream in/out *after* this
	// seed pass (the common case — WP cells load on demand, not at map-open) are picked up. WP loads
	// actors via ULevel::AddLoadedActor, bypassing UWorld::SpawnActor, so GEngine->OnLevelActorAdded
	// never fires for them; these per-level events are the canonical signal (mirrors engine ActorHierarchy).
	BindLevelLoadedActorEvents();
}

void UArcMassOutlinerRowManager::BindLevelLoadedActorEvents()
{
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	ULevel* PersistentLevel = EditorWorld ? EditorWorld->PersistentLevel : nullptr;

	// Already bound to this exact level — nothing to do.
	if (BoundLevel.Get() == PersistentLevel && LoadedActorAddedHandle.IsValid())
	{
		return;
	}

	// Unbind the previously-bound level (it may be a stale/destroyed world after a map change).
	if (ULevel* OldLevel = BoundLevel.Get())
	{
		if (LoadedActorAddedHandle.IsValid())
		{
			OldLevel->OnLoadedActorAddedToLevelEvent.Remove(LoadedActorAddedHandle);
		}
		if (LoadedActorRemovedHandle.IsValid())
		{
			OldLevel->OnLoadedActorRemovedFromLevelEvent.Remove(LoadedActorRemovedHandle);
		}
	}
	LoadedActorAddedHandle.Reset();
	LoadedActorRemovedHandle.Reset();
	BoundLevel.Reset();

	if (!PersistentLevel)
	{
		return;
	}

	BoundLevel = PersistentLevel;
	LoadedActorAddedHandle = PersistentLevel->OnLoadedActorAddedToLevelEvent.AddUObject(
		this, &UArcMassOutlinerRowManager::OnLoadedActorAddedToLevel);
	LoadedActorRemovedHandle = PersistentLevel->OnLoadedActorRemovedFromLevelEvent.AddUObject(
		this, &UArcMassOutlinerRowManager::OnLoadedActorRemovedFromLevel);
}

void UArcMassOutlinerRowManager::OnLoadedActorAddedToLevel(AActor& InActor)
{
	if (AArcPlacedEntityPartitionActor* PartitionActor = Cast<AArcPlacedEntityPartitionActor>(&InActor))
	{
		RebuildActorRows(PartitionActor);
	}
}

void UArcMassOutlinerRowManager::OnLoadedActorRemovedFromLevel(AActor& InActor)
{
	AArcPlacedEntityPartitionActor* PartitionActor = Cast<AArcPlacedEntityPartitionActor>(&InActor);
	if (!PartitionActor)
	{
		return;
	}

	TearDownActorRows(PartitionActor);

	TWeakObjectPtr<AArcPlacedEntityPartitionActor> WeakActor(PartitionActor);
	FArcOutlinerActorState* State = ActorStates.Find(WeakActor);
	if (State && State->ChangedHandle.IsValid())
	{
		PartitionActor->OnEditorStateChanged.Remove(State->ChangedHandle);
		State->ChangedHandle.Reset();
	}
	ActorStates.Remove(WeakActor);
}

void UArcMassOutlinerRowManager::OnMapOpened(const FString& /*Filename*/, bool /*bAsTemplate*/)
{
	// The previous world's actor rows are gone with the world; drop stale bookkeeping then re-seed.
	ActorStates.Empty();
	SeedActorsInEditorWorld();
}

void UArcMassOutlinerRowManager::RebuildActorRows(AArcPlacedEntityPartitionActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Step 1: tear down any existing rows for this actor.
	TearDownActorRows(Actor);

	// Step 2: bind the per-actor state-changed delegate if not already bound.
	TWeakObjectPtr<AArcPlacedEntityPartitionActor> WeakActor(Actor);
	FArcOutlinerActorState& State = ActorStates.FindOrAdd(WeakActor);
	if (!State.ChangedHandle.IsValid())
	{
		State.ChangedHandle = Actor->OnEditorStateChanged.AddUObject(
			this, &UArcMassOutlinerRowManager::OnActorStateChanged);
	}

	// Step 3: acquire TEDS providers.
	UE::Editor::DataStorage::ICoreProvider* Storage =
		UE::Editor::DataStorage::GetMutableDataStorageFeature<UE::Editor::DataStorage::ICoreProvider>(
			UE::Editor::DataStorage::StorageFeatureName);
	UE::Editor::DataStorage::ICompatibilityProvider* Compat =
		UE::Editor::DataStorage::GetMutableDataStorageFeature<UE::Editor::DataStorage::ICompatibilityProvider>(
			UE::Editor::DataStorage::CompatibilityFeatureName);

	if (!Storage || !Compat)
	{
		return;
	}

	// Step 4: find the actor's existing TEDS row. If not yet registered, bail — we'll be called again on Rebuilt.
	UE::Editor::DataStorage::RowHandle ActorRow = Compat->FindRowWithCompatibleObjectExplicit(Actor);
	if (ActorRow == UE::Editor::DataStorage::InvalidRowHandle)
	{
		// Actor's TEDS row isn't up yet (common right after a stream-in). We've bound OnEditorStateChanged
		// above, so the subsequent Rebuilt broadcast will retry. Expected — no log.
		return;
	}

	// Step 5: find the editor object hierarchy.
	UE::Editor::DataStorage::FHierarchyHandle H =
		Storage->FindHierarchyByName(UTedsEditorHierarchyFactory::EditorObjectHierarchyName);
	if (!Storage->IsValidHierarchyHandle(H))
	{
		UE_LOGF(LogTemp, Warning,"UArcMassOutlinerRowManager::RebuildActorRows(%ls): EditorObjectHierarchy not valid", *GetNameSafe(Actor));
		return;
	}

	// Step 6: locate the outliner factory to get our table handles.
	UArcPlacedEntityOutlinerFactory* Factory = Storage->FindFactory<UArcPlacedEntityOutlinerFactory>();
	if (!Factory)
	{
		UE_LOGF(LogTemp, Warning,"UArcMassOutlinerRowManager::RebuildActorRows(%ls): UArcPlacedEntityOutlinerFactory not found", *GetNameSafe(Actor));
		return;
	}

	// Step 7: build config-group rows and instance rows for every entry.
	const TArray<FArcPlacedEntityEntry>& Entries = Actor->GetEntries();

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];

		// --- Config-group row ---
		UE::Editor::DataStorage::RowHandle ConfigRow = Storage->AddRow(Factory->ConfigGroupTable);

		FTypedElementLabelColumn LabelColumn;
		LabelColumn.Label = FString::Printf(
			TEXT("%s (%d)"),
			*GetNameSafe(Entry.EntityConfig),
			Entry.InstanceTransforms.Num());
		Storage->AddColumn(ConfigRow, MoveTemp(LabelColumn));

		FTypedElementWorldColumn WorldColumn;
		WorldColumn.World = Actor->GetWorld();
		Storage->AddColumn(ConfigRow, MoveTemp(WorldColumn));

		// Satisfies the outliner query's .Any<> gate so this synthetic row appears under the partition actor.
		FTypedElementUObjectColumn ObjectColumn;
		ObjectColumn.Object = Actor;
		Storage->AddColumn(ConfigRow, MoveTemp(ObjectColumn));

		FArcPlacedEntityOutlinerInstanceColumn ConfigIdentityColumn;
		ConfigIdentityColumn.PartitionActor = Actor;
		ConfigIdentityColumn.EntryIndex = EntryIndex;
		ConfigIdentityColumn.TransformIndex = INDEX_NONE;
		Storage->AddColumn(ConfigRow, MoveTemp(ConfigIdentityColumn));

		Storage->SetParentRow(H, ConfigRow, ActorRow);

		State.ConfigGroupRows.Add(EntryIndex, ConfigRow);

		// --- Per-instance rows ---
		for (int32 TransformIndex = 0; TransformIndex < Entry.InstanceTransforms.Num(); ++TransformIndex)
		{
			UE::Editor::DataStorage::RowHandle InstRow = Storage->AddRow(Factory->InstanceTable);

			FTypedElementLabelColumn InstLabelColumn;
			InstLabelColumn.Label = FString::Printf(
				TEXT("%s [%d]"),
				*GetNameSafe(Entry.EntityConfig),
				TransformIndex);
			Storage->AddColumn(InstRow, MoveTemp(InstLabelColumn));

			FTypedElementWorldColumn InstWorldColumn;
			InstWorldColumn.World = Actor->GetWorld();
			Storage->AddColumn(InstRow, MoveTemp(InstWorldColumn));

			FTypedElementUObjectColumn InstObjectColumn;
			InstObjectColumn.Object = Actor;
			Storage->AddColumn(InstRow, MoveTemp(InstObjectColumn));

			FArcPlacedEntityOutlinerInstanceColumn InstIdentityColumn;
			InstIdentityColumn.PartitionActor = Actor;
			InstIdentityColumn.EntryIndex = EntryIndex;
			InstIdentityColumn.TransformIndex = TransformIndex;
			Storage->AddColumn(InstRow, MoveTemp(InstIdentityColumn));

			// Typed-element handle for selection bridging — only add if valid.
			FArcMassEntityElementId ElementId;
			ElementId.PartitionActor = Actor;
			ElementId.EntryIndex = EntryIndex;
			ElementId.TransformIndex = TransformIndex;

			FTypedElementHandle ElementHandle =
				ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(ElementId, /*bAllowCreate=*/true);
			if (ElementHandle.IsSet())
			{
				FTedsTypedElementColumn HandleColumn;
				HandleColumn.Handle = MoveTemp(ElementHandle);
				Storage->AddColumn(InstRow, MoveTemp(HandleColumn));
			}

			Storage->SetParentRow(H, InstRow, ConfigRow);

			State.InstanceRows.Add(TPair<int32, int32>(EntryIndex, TransformIndex), InstRow);
		}
	}
}

void UArcMassOutlinerRowManager::TearDownActorRows(AArcPlacedEntityPartitionActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	TWeakObjectPtr<AArcPlacedEntityPartitionActor> WeakActor(Actor);
	FArcOutlinerActorState* State = ActorStates.Find(WeakActor);
	if (!State)
	{
		return;
	}

	UE::Editor::DataStorage::ICoreProvider* Storage =
		UE::Editor::DataStorage::GetMutableDataStorageFeature<UE::Editor::DataStorage::ICoreProvider>(
			UE::Editor::DataStorage::StorageFeatureName);

	if (!Storage)
	{
		// Can't remove rows without storage; clear the maps anyway so they don't accumulate stale handles.
		State->ConfigGroupRows.Empty();
		State->InstanceRows.Empty();
		return;
	}

	TArray<UE::Editor::DataStorage::RowHandle> RowsToRemove;
	ArcMassOutlinerRowManagerImpl::GatherRowHandles(*State, RowsToRemove);

	if (RowsToRemove.Num() > 0)
	{
		Storage->BatchRemoveRows(TConstArrayView<UE::Editor::DataStorage::RowHandle>(RowsToRemove));
	}

	State->ConfigGroupRows.Empty();
	State->InstanceRows.Empty();
	// ChangedHandle is intentionally left untouched here — the caller decides whether to unbind it.
}

void UArcMassOutlinerRowManager::OnActorStateChanged(
	AArcPlacedEntityPartitionActor* Actor,
	EArcPlacedEntityOutlinerEvent Event)
{
	switch (Event)
	{
		case EArcPlacedEntityOutlinerEvent::Rebuilt:
		{
			RebuildActorRows(Actor);
			break;
		}
		case EArcPlacedEntityOutlinerEvent::TornDown:
		{
			TearDownActorRows(Actor);
			break;
		}
		case EArcPlacedEntityOutlinerEvent::Destroyed:
		{
			TearDownActorRows(Actor);

			TWeakObjectPtr<AArcPlacedEntityPartitionActor> WeakActor(Actor);
			FArcOutlinerActorState* State = ActorStates.Find(WeakActor);
			if (State && State->ChangedHandle.IsValid())
			{
				Actor->OnEditorStateChanged.Remove(State->ChangedHandle);
				State->ChangedHandle.Reset();
			}
			ActorStates.Remove(WeakActor);
			break;
		}
		default:
		{
			break;
		}
	}
}
