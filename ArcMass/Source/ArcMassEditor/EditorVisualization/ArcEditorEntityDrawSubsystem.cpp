// Copyright Lukasz Baran. All Rights Reserved.

#include "EditorVisualization/ArcEditorEntityDrawSubsystem.h"
#include "EditorVisualization/ArcMassEditorDebugDrawSettings.h"
#include "EditorVisualization/ArcMassEditorVisualizationRegistry.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcAlwaysLoadedEntityActor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"

void UArcEditorEntityDrawSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (IsRunningCommandlet())
	{
		return;
	}

	RebuildCategories();
	DrawContributorsChangedHandle = FArcMassEditorVisualizationRegistry::Get().OnDrawContributorsChanged().AddUObject(
		this, &UArcEditorEntityDrawSubsystem::HandleDrawContributorsChanged);

	if (GEngine != nullptr && GEngine->IsInitialized())
	{
		BindAll();
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddUObject(this, &UArcEditorEntityDrawSubsystem::BindAll);
	}
}

void UArcEditorEntityDrawSubsystem::BindAll()
{
	OnBeginFrameHandle = FCoreDelegates::OnBeginFrame.AddUObject(this, &UArcEditorEntityDrawSubsystem::Pump);

	MapOpenedHandle = FEditorDelegates::OnMapOpened.AddUObject(this, &UArcEditorEntityDrawSubsystem::HandleMapOpened);
	MapChangeHandle = FEditorDelegates::MapChange.AddUObject(this, &UArcEditorEntityDrawSubsystem::HandleMapChange);
	WorldInitializedHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this, &UArcEditorEntityDrawSubsystem::HandleWorldInitialized);
	WorldFinishDestroyHandle = FWorldDelegates::OnPreWorldFinishDestroy.AddUObject(
		this, &UArcEditorEntityDrawSubsystem::HandleWorldFinishDestroy);

	LevelAddedToWorldHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(
		this, &UArcEditorEntityDrawSubsystem::HandleLevelChanged);
	LevelRemovedFromWorldHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(
		this, &UArcEditorEntityDrawSubsystem::HandleLevelChanged);

	if (GEngine != nullptr)
	{
		ActorAddedHandle = GEngine->OnLevelActorAdded().AddUObject(this, &UArcEditorEntityDrawSubsystem::HandleActorAdded);
		ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &UArcEditorEntityDrawSubsystem::HandleActorDeleted);
	}

	PostUndoRedoHandle = FEditorDelegates::PostUndoRedo.AddUObject(this, &UArcEditorEntityDrawSubsystem::HandlePostUndoRedo);

	// AddInstance only calls Modify(), never PostEditChangeProperty, so placing an entity dirties nothing without this.
	ObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddUObject(
		this, &UArcEditorEntityDrawSubsystem::HandleObjectModified);
	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(
		this, &UArcEditorEntityDrawSubsystem::OnObjectPropertyChanged);
	InstanceTransformChangedHandle = FArcPlacedEntityEditorEvents::OnInstanceTransformChanged.AddUObject(
		this, &UArcEditorEntityDrawSubsystem::HandleInstanceTransformChanged);

	RebindWorld();
}

void UArcEditorEntityDrawSubsystem::RebindWorld()
{
	ActiveWorld = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;

	SubscribeLevels();

	bCollectorDirty = true;
}

void UArcEditorEntityDrawSubsystem::SubscribeLevels()
{
	UnsubscribeLevels();

	UWorld* World = ActiveWorld.Get();
	if (World == nullptr)
	{
		return;
	}

	for (ULevel* Level : World->GetLevels())
	{
		if (Level == nullptr)
		{
			continue;
		}

		FArcEditorDrawLevelSubscription& Subscription = LevelSubscriptions.AddDefaulted_GetRef();
		Subscription.Level = Level;
		Subscription.ActorAddedHandle = Level->OnLoadedActorAddedToLevelEvent.AddUObject(
			this, &UArcEditorEntityDrawSubsystem::HandleLoadedActorAdded);
		Subscription.ActorRemovedHandle = Level->OnLoadedActorRemovedFromLevelEvent.AddUObject(
			this, &UArcEditorEntityDrawSubsystem::HandleLoadedActorRemoved);
	}
}

void UArcEditorEntityDrawSubsystem::UnsubscribeLevels()
{
	for (FArcEditorDrawLevelSubscription& Subscription : LevelSubscriptions)
	{
		ULevel* Level = Subscription.Level.Get();
		if (Level == nullptr)
		{
			continue;
		}

		Level->OnLoadedActorAddedToLevelEvent.Remove(Subscription.ActorAddedHandle);
		Level->OnLoadedActorRemovedFromLevelEvent.Remove(Subscription.ActorRemovedHandle);
	}

	LevelSubscriptions.Reset();
}

void UArcEditorEntityDrawSubsystem::Deinitialize()
{
	FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
	PostEngineInitHandle.Reset();

	FCoreDelegates::OnBeginFrame.Remove(OnBeginFrameHandle);
	OnBeginFrameHandle.Reset();

	FEditorDelegates::OnMapOpened.Remove(MapOpenedHandle);
	MapOpenedHandle.Reset();
	FEditorDelegates::MapChange.Remove(MapChangeHandle);
	MapChangeHandle.Reset();
	FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializedHandle);
	WorldInitializedHandle.Reset();
	FWorldDelegates::OnPreWorldFinishDestroy.Remove(WorldFinishDestroyHandle);
	WorldFinishDestroyHandle.Reset();

	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedToWorldHandle);
	LevelAddedToWorldHandle.Reset();
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedFromWorldHandle);
	LevelRemovedFromWorldHandle.Reset();

	if (GEngine != nullptr)
	{
		GEngine->OnLevelActorAdded().Remove(ActorAddedHandle);
		GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
	}
	ActorAddedHandle.Reset();
	ActorDeletedHandle.Reset();

	UnsubscribeLevels();

	FEditorDelegates::PostUndoRedo.Remove(PostUndoRedoHandle);
	PostUndoRedoHandle.Reset();

	FCoreUObjectDelegates::OnObjectModified.Remove(ObjectModifiedHandle);
	ObjectModifiedHandle.Reset();
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	PropertyChangedHandle.Reset();
	FArcPlacedEntityEditorEvents::OnInstanceTransformChanged.Remove(InstanceTransformChangedHandle);
	InstanceTransformChangedHandle.Reset();

	FArcMassEditorVisualizationRegistry::Get().OnDrawContributorsChanged().Remove(DrawContributorsChangedHandle);
	DrawContributorsChangedHandle.Reset();

	Collector.Reset();
	bCollectorDirty = false;
	ActiveWorld.Reset();

	Super::Deinitialize();
}

void UArcEditorEntityDrawSubsystem::RebuildCategories()
{
	TArray<TSharedPtr<IArcEditorEntityDrawContributor>> Contributors;
	FArcMassEditorVisualizationRegistry::Get().GetDrawContributors(Contributors);

	TMap<FName, EArcDrawCategoryKind> RebuiltKinds;
	RebuiltKinds.Reserve(Contributors.Num());

	for (const TSharedPtr<IArcEditorEntityDrawContributor>& Contributor : Contributors)
	{
		const FName Category = Contributor->GetCategory();
		const EArcDrawCategoryKind Kind = Contributor->GetCategoryKind();

		EArcDrawCategoryKind* ExistingKind = RebuiltKinds.Find(Category);
		if (ExistingKind == nullptr)
		{
			RebuiltKinds.Add(Category, Kind);
		}
		else if (Kind == EArcDrawCategoryKind::Representation)
		{
			*ExistingKind = EArcDrawCategoryKind::Representation;
		}
	}

	const UArcMassEditorDebugDrawSettings* Settings = GetDefault<UArcMassEditorDebugDrawSettings>();

	TMap<FName, bool> RebuiltState;
	RebuiltState.Reserve(RebuiltKinds.Num());

	for (const TPair<FName, EArcDrawCategoryKind>& KindPair : RebuiltKinds)
	{
		if (KindPair.Value != EArcDrawCategoryKind::Detail)
		{
			continue;
		}

		const bool* SavedValue = Settings->CategoryToggleState.Find(KindPair.Key);
		if (SavedValue != nullptr)
		{
			RebuiltState.Add(KindPair.Key, *SavedValue);
			continue;
		}

		const bool* UserValue = CategoryToggleState.Find(KindPair.Key);
		RebuiltState.Add(KindPair.Key, UserValue != nullptr ? *UserValue : false);
	}

	CategoryKinds = MoveTemp(RebuiltKinds);
	CategoryToggleState = MoveTemp(RebuiltState);
}

void UArcEditorEntityDrawSubsystem::HandleDrawContributorsChanged()
{
	RebuildCategories();
	bCollectorDirty = true;
}

void UArcEditorEntityDrawSubsystem::SetCategoryEnabled(FName Category, bool bEnabled)
{
	bool* Existing = CategoryToggleState.Find(Category);
	if (Existing == nullptr)
	{
		return;
	}

	*Existing = bEnabled;

	UArcMassEditorDebugDrawSettings* Settings = GetMutableDefault<UArcMassEditorDebugDrawSettings>();
	Settings->CategoryToggleState.FindOrAdd(Category) = bEnabled;
	Settings->SaveConfig();

	RefreshShapes();
}

bool UArcEditorEntityDrawSubsystem::IsCategoryEnabled(FName Category) const
{
	if (GetCategoryKind(Category) == EArcDrawCategoryKind::Representation)
	{
		return true;
	}

	const bool* bEnabled = CategoryToggleState.Find(Category);
	return bEnabled != nullptr ? *bEnabled : false;
}

EArcDrawCategoryKind UArcEditorEntityDrawSubsystem::GetCategoryKind(FName Category) const
{
	const EArcDrawCategoryKind* Kind = CategoryKinds.Find(Category);
	return Kind != nullptr ? *Kind : EArcDrawCategoryKind::Detail;
}

TArray<FName> UArcEditorEntityDrawSubsystem::GetAllCategories() const
{
	TArray<FName> Categories;
	CategoryKinds.GetKeys(Categories);
	return Categories;
}

void UArcEditorEntityDrawSubsystem::HandleMapOpened(const FString& /*Filename*/, bool /*bAsTemplate*/)
{
	RebindWorld();
}

void UArcEditorEntityDrawSubsystem::HandleMapChange(uint32 /*MapChangeFlags*/)
{
	RebindWorld();
}

void UArcEditorEntityDrawSubsystem::HandleWorldInitialized(UWorld* World, const UWorld::InitializationValues /*IVS*/)
{
	if (World != nullptr && World->WorldType == EWorldType::Editor)
	{
		RebindWorld();
	}
}

void UArcEditorEntityDrawSubsystem::HandleWorldFinishDestroy(UWorld* World)
{
	if (World == nullptr || !ActiveWorld.HasSameIndexAndSerialNumber(TWeakObjectPtr<UWorld>(World)))
	{
		return;
	}

	UnsubscribeLevels();
	ActiveWorld.Reset();
	bCollectorDirty = true;
}

void UArcEditorEntityDrawSubsystem::HandleActorAdded(AActor* Actor)
{
	DirtyIfPlacedEntity(Actor);
}

void UArcEditorEntityDrawSubsystem::HandleActorDeleted(AActor* Actor)
{
	DirtyIfPlacedEntity(Actor);
}

void UArcEditorEntityDrawSubsystem::HandleLoadedActorAdded(AActor& Actor)
{
	DirtyIfPlacedEntity(&Actor);
}

void UArcEditorEntityDrawSubsystem::HandleLoadedActorRemoved(AActor& Actor)
{
	DirtyIfPlacedEntity(&Actor);
}

void UArcEditorEntityDrawSubsystem::HandleLevelChanged(ULevel* /*Level*/, UWorld* World)
{
	if (World == nullptr || !ActiveWorld.HasSameIndexAndSerialNumber(TWeakObjectPtr<UWorld>(World)))
	{
		return;
	}

	SubscribeLevels();
	bCollectorDirty = true;
}

void UArcEditorEntityDrawSubsystem::HandlePostUndoRedo()
{
	bCollectorDirty = true;
}

void UArcEditorEntityDrawSubsystem::HandleObjectModified(UObject* Object)
{
	DirtyIfPlacedEntity(Object);
}

void UArcEditorEntityDrawSubsystem::HandleInstanceTransformChanged()
{
	bCollectorDirty = true;
}

void UArcEditorEntityDrawSubsystem::DirtyIfPlacedEntity(const UObject* Object)
{
	if (Object == nullptr
		|| (!Object->IsA(AArcPlacedEntityPartitionActor::StaticClass())
			&& !Object->IsA(AArcAlwaysLoadedEntityActor::StaticClass())))
	{
		return;
	}

	const UWorld* ObjectWorld = Object->GetWorld();
	if (ObjectWorld == nullptr || ObjectWorld != ActiveWorld.Get())
	{
		return;
	}

	bCollectorDirty = true;
}

void UArcEditorEntityDrawSubsystem::Pump()
{
	// UE::DebugDraw pools are process-global, so a PIE/Simulate session cannot be scoped out per viewport.
	if (GEditor == nullptr || GEditor->PlayWorld != nullptr)
	{
		return;
	}

	if (!ActiveWorld.IsValid())
	{
		return;
	}

	if (bCollectorDirty)
	{
		RefreshShapes();
	}

	Collector.Submit(ActiveWorld.Get());
}

void UArcEditorEntityDrawSubsystem::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& /*Event*/)
{
	if (Object == nullptr)
	{
		return;
	}

	if (Object->IsA(AArcPlacedEntityPartitionActor::StaticClass())
		|| Object->IsA(AArcAlwaysLoadedEntityActor::StaticClass())
		|| Object->IsA(UMassEntityConfigAsset::StaticClass())
		|| Object->IsA(UMassEntityTraitBase::StaticClass()))
	{
		bCollectorDirty = true;
	}
}

void UArcEditorEntityDrawSubsystem::BuildContext(FArcEditorDrawContext& OutContext) const
{
	UWorld* World = ActiveWorld.Get();
	OutContext.World = World;
	OutContext.AllPlacedEntities.Reset();

	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AArcPlacedEntityPartitionActor> It(World); It; ++It)
	{
		AArcPlacedEntityPartitionActor* PartitionActor = *It;
		for (const FArcPlacedEntityEntry& Entry : PartitionActor->Entries)
		{
			if (!Entry.EntityConfig || Entry.InstanceTransforms.IsEmpty())
			{
				continue;
			}

			FArcEditorDrawContext::FPlacedEntityInfo& Info = OutContext.AllPlacedEntities.AddDefaulted_GetRef();
			Info.Config = Entry.EntityConfig;

			const FTransform ActorTransform = PartitionActor->GetActorTransform();
			const FTransform EntryPivot = PartitionActor->GetEntryComponentTransform(Entry);
			Info.Transforms.Reserve(Entry.InstanceTransforms.Num());
			for (const FTransform& LocalTransform : Entry.InstanceTransforms)
			{
				Info.Transforms.Add(EntryPivot * (LocalTransform * ActorTransform));
			}
		}
	}

	for (TActorIterator<AArcAlwaysLoadedEntityActor> It(World); It; ++It)
	{
		AArcAlwaysLoadedEntityActor* AlwaysLoadedActor = *It;
		for (const FArcPlacedEntityEntry& Entry : AlwaysLoadedActor->Entries)
		{
			if (!Entry.EntityConfig || Entry.InstanceTransforms.IsEmpty())
			{
				continue;
			}

			FArcEditorDrawContext::FPlacedEntityInfo& Info = OutContext.AllPlacedEntities.AddDefaulted_GetRef();
			Info.Config = Entry.EntityConfig;

			const FTransform ActorTransform = AlwaysLoadedActor->GetActorTransform();
			const FTransform EntryPivot = AlwaysLoadedActor->GetEntryComponentTransform(Entry);
			Info.Transforms.Reserve(Entry.InstanceTransforms.Num());
			for (const FTransform& LocalTransform : Entry.InstanceTransforms)
			{
				Info.Transforms.Add(EntryPivot * (LocalTransform * ActorTransform));
			}
		}
	}
}

void UArcEditorEntityDrawSubsystem::RefreshShapes()
{
	Collector.Reset();

	FArcEditorDrawContext Context;
	BuildContext(Context);

	TArray<TSharedPtr<IArcEditorEntityDrawContributor>> Contributors;
	FArcMassEditorVisualizationRegistry::Get().GetDrawContributors(Contributors);

	for (const TSharedPtr<IArcEditorEntityDrawContributor>& Contributor : Contributors)
	{
		const FName Category = Contributor->GetCategory();
		if (!IsCategoryEnabled(Category))
		{
			continue;
		}

		const TSubclassOf<UMassEntityTraitBase> TraitClass = Contributor->GetTraitClass();

		if (!TraitClass)
		{
			Contributor->CollectShapes(Collector, nullptr, {}, Context);
			continue;
		}

		for (const FArcEditorDrawContext::FPlacedEntityInfo& Info : Context.AllPlacedEntities)
		{
			const UMassEntityTraitBase* Trait = Info.Config->FindTrait(TraitClass);
			if (Trait)
			{
				Contributor->CollectShapes(Collector, Trait, Info.Transforms, Context);
			}
		}
	}

	bCollectorDirty = false;
}
