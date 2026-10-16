// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Engine/World.h"
#include "ArcEditorEntityDrawContributor.h"
#include "ArcEditorDrawCollector.h"
#include "ArcEditorEntityDrawSubsystem.generated.h"

class ULevel;

struct FArcEditorDrawLevelSubscription
{
	TWeakObjectPtr<ULevel> Level;
	FDelegateHandle ActorAddedHandle;
	FDelegateHandle ActorRemovedHandle;
};

UCLASS()
class ARCMASSEDITOR_API UArcEditorEntityDrawSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void SetCategoryEnabled(FName Category, bool bEnabled);
	bool IsCategoryEnabled(FName Category) const;
	EArcDrawCategoryKind GetCategoryKind(FName Category) const;
	TArray<FName> GetAllCategories() const;

	void RefreshShapes();

private:
	void BindAll();
	void RebindWorld();
	void SubscribeLevels();
	void UnsubscribeLevels();

	void BuildContext(FArcEditorDrawContext& OutContext) const;

	void Pump();

	void HandleMapOpened(const FString& Filename, bool bAsTemplate);
	void HandleMapChange(uint32 MapChangeFlags);
	void HandleWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS);
	void HandleWorldFinishDestroy(UWorld* World);
	void HandleActorAdded(AActor* Actor);
	void HandleActorDeleted(AActor* Actor);
	void HandleLoadedActorAdded(AActor& Actor);
	void HandleLoadedActorRemoved(AActor& Actor);
	void HandleLevelChanged(ULevel* Level, UWorld* World);
	void HandlePostUndoRedo();
	void HandleObjectModified(UObject* Object);
	void HandleInstanceTransformChanged();
	void OnObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);

	void DirtyIfPlacedEntity(const UObject* Object);

	void RebuildCategories();
	void HandleDrawContributorsChanged();

	TMap<FName, bool> CategoryToggleState;

	TMap<FName, EArcDrawCategoryKind> CategoryKinds;

	FArcEditorDrawCollector Collector;

	// Weak and unreflected: a hard reference keeps the previous editor world alive and UEditorEngine::CheckForWorldGCLeaks fatals on the next map load (EditorServer.cpp:1912).
	TWeakObjectPtr<UWorld> ActiveWorld;

	TArray<FArcEditorDrawLevelSubscription> LevelSubscriptions;

	FDelegateHandle PostEngineInitHandle;
	FDelegateHandle OnBeginFrameHandle;
	FDelegateHandle MapOpenedHandle;
	FDelegateHandle MapChangeHandle;
	FDelegateHandle WorldInitializedHandle;
	FDelegateHandle WorldFinishDestroyHandle;
	FDelegateHandle ActorAddedHandle;
	FDelegateHandle ActorDeletedHandle;
	FDelegateHandle LevelAddedToWorldHandle;
	FDelegateHandle LevelRemovedFromWorldHandle;
	FDelegateHandle PostUndoRedoHandle;
	FDelegateHandle ObjectModifiedHandle;
	FDelegateHandle PropertyChangedHandle;
	FDelegateHandle InstanceTransformChangedHandle;
	FDelegateHandle DrawContributorsChangedHandle;

	bool bCollectorDirty = false;
};
