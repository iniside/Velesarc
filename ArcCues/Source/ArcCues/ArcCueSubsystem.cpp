// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"

#include "ArcCueBehavior.h"
#include "ArcCueDefinition.h"
#include "ArcCuesModule.h"

UArcCueSubsystem* UArcCueSubsystem::Get(const UObject* WorldContext)
{
	if (WorldContext == nullptr)
	{
		return nullptr;
	}
	UWorld* World = WorldContext->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	return World->GetSubsystem<UArcCueSubsystem>();
}

void UArcCueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RebuildDiscoveryCache();

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (AssetRegistry != nullptr)
	{
		OnAssetAddedHandle = AssetRegistry->OnAssetAdded().AddUObject(this, &UArcCueSubsystem::OnAssetAdded);
		OnAssetRemovedHandle = AssetRegistry->OnAssetRemoved().AddUObject(this, &UArcCueSubsystem::OnAssetRemoved);
	}
}

void UArcCueSubsystem::Deinitialize()
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (AssetRegistry != nullptr)
	{
		if (OnAssetAddedHandle.IsValid())
		{
			AssetRegistry->OnAssetAdded().Remove(OnAssetAddedHandle);
		}
		if (OnAssetRemovedHandle.IsValid())
		{
			AssetRegistry->OnAssetRemoved().Remove(OnAssetRemovedHandle);
		}
	}

	// World is tearing down. Mass's UMassEntitySubsystem may have already deinit'd
	// — calling GetMutableEntityManager() on it asserts. Active cue entities and
	// any UWorld-bound resources die with the world, so behavior OnRemoved hooks
	// have nothing to clean up. Just drop our state.
	ActiveCues.Empty();
	CuesByTarget.Empty();
	CuesByOwner.Empty();
	PendingOperations.Empty();
	PendingEntityDestroys.Empty();
	TagToDefinitions.Empty();

	Super::Deinitialize();
}

TStatId UArcCueSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcCueSubsystem, STATGROUP_Tickables);
}

void UArcCueSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	check(IsInGameThread());

	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = (World != nullptr) ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManager = (MassSubsystem != nullptr) ? &MassSubsystem->GetMutableEntityManager() : nullptr;
	FlushPendingEntityDestroys(EntityManager);

	if (ActiveCues.Num() == 0)
	{
		return;
	}

	TArray<FArcCueHandle> ActiveHandles;
	ActiveCues.GenerateKeyArray(ActiveHandles);

	BeginCallbackDispatch();
	for (const FArcCueHandle& Handle : ActiveHandles)
	{
		UArcActiveCueInstance* Active = ActiveCues.FindRef(Handle);
		if (Active == nullptr)
		{
			continue;
		}

		Active->ElapsedTime += DeltaTime;

		const UArcCueDefinition* Definition = Active->Definition.Get();
		if (Definition == nullptr)
		{
			continue;
		}

		FArcCueContext Context(Active->Parameters);
		Context.Handle = Active->Handle;
		Context.EntityManager = EntityManager;
		Context.World = World;
		Context.Definition = Definition;

		const int32 NumBehaviors = Definition->Behaviors.Num();
		const int32 NumStates = Active->BehaviorStates.Num();
		const int32 IterCount = FMath::Min(NumBehaviors, NumStates);

		for (int32 Index = 0; Index < IterCount; ++Index)
		{
			Context.BehaviorIndex = Index;
			TInstancedStruct<FArcCueBehaviorState>& StateSlot = Active->BehaviorStates[Index];
			if (!StateSlot.IsValid())
			{
				continue;
			}

			const FInstancedStruct& BehaviorSlot = Definition->Behaviors[Index];
			const FArcCueBehavior* Behavior = BehaviorSlot.GetPtr<FArcCueBehavior>();
			if (Behavior == nullptr || !Behavior->WantsTick())
			{
				continue;
			}

			Behavior->OnTick(Context, StateSlot.GetMutable(), DeltaTime);
		}
	}
	EndCallbackDispatch();
}

void UArcCueSubsystem::ExecuteCue(FGameplayTag CueTag, const FArcCueParameters& Params)
{
	check(IsInGameThread());

	if (!CueTag.IsValid())
	{
		return;
	}

	TArray<UArcCueDefinition*> Matches;
	GatherMatchingDefinitions(CueTag, Params.Tags, Matches);
	if (Matches.Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = (World != nullptr) ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManager = (MassSubsystem != nullptr) ? &MassSubsystem->GetMutableEntityManager() : nullptr;

	BeginCallbackDispatch();
	for (UArcCueDefinition* Definition : Matches)
	{
		if (Definition == nullptr)
		{
			continue;
		}

		FArcCueContext Context(Params);
		Context.Handle = FArcCueHandle::Invalid();
		Context.EntityManager = EntityManager;
		Context.World = World;
		Context.Definition = Definition;

		for (int32 Index = 0; Index < Definition->Behaviors.Num(); ++Index)
		{
			Context.BehaviorIndex = Index;
			const FInstancedStruct& BehaviorSlot = Definition->Behaviors[Index];
			const FArcCueBehavior* Behavior = BehaviorSlot.GetPtr<FArcCueBehavior>();
			if (Behavior != nullptr)
			{
				Behavior->OnExecute(Context);
			}
		}
	}
	EndCallbackDispatch();
}

void UArcCueSubsystem::ExecuteCue(UArcCueDefinition* Definition, const FArcCueParameters& Params)
{
	check(IsInGameThread());

	if (Definition == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = (World != nullptr) ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManager = (MassSubsystem != nullptr) ? &MassSubsystem->GetMutableEntityManager() : nullptr;

	FArcCueContext Context(Params);
	Context.Handle = FArcCueHandle::Invalid();
	Context.EntityManager = EntityManager;
	Context.World = World;
	Context.Definition = Definition;

	BeginCallbackDispatch();
	for (int32 Index = 0; Index < Definition->Behaviors.Num(); ++Index)
	{
		Context.BehaviorIndex = Index;
		const FInstancedStruct& BehaviorSlot = Definition->Behaviors[Index];
		const FArcCueBehavior* Behavior = BehaviorSlot.GetPtr<FArcCueBehavior>();
		if (Behavior != nullptr)
		{
			Behavior->OnExecute(Context);
		}
	}
	EndCallbackDispatch();
}

FArcCueHandle UArcCueSubsystem::AddCue(FGameplayTag CueTag, const FArcCueParameters& Params)
{
	check(IsInGameThread());

	if (!CueTag.IsValid())
	{
		return FArcCueHandle::Invalid();
	}

	TArray<UArcCueDefinition*> Matches;
	GatherMatchingDefinitions(CueTag, Params.Tags, Matches);
	if (Matches.Num() != 1)
	{
		if (Matches.Num() > 1)
		{
			UE_LOGF(LogArcCues, Error, "Persistent cue tag '%ls' matched %d definitions; use the exact-definition AddCue overload.",
				*CueTag.ToString(), Matches.Num());
		}
		return FArcCueHandle::Invalid();
	}

	return AddCue(Matches[0], Params);
}

FArcCueHandle UArcCueSubsystem::AddCue(UArcCueDefinition* Definition, const FArcCueParameters& Params)
{
	check(IsInGameThread());

	if (Definition == nullptr)
	{
		return FArcCueHandle::Invalid();
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = (World != nullptr) ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManager = (MassSubsystem != nullptr) ? &MassSubsystem->GetMutableEntityManager() : nullptr;

	const FArcCueHandle Handle = AllocateHandle();

	UArcActiveCueInstance* Active = NewObject<UArcActiveCueInstance>(this);
	Active->Handle = Handle;
	Active->Definition = Definition;
	Active->Parameters = Params;
	Active->Parameters.EffectSpec = nullptr;
	Active->BehaviorStates.SetNum(Definition->Behaviors.Num());
	ActiveCues.Add(Handle, Active);
	UE_LOGF(LogArcCues, Log, "[CueLifecycle] Added: Handle=%u Definition=%ls World=%ls Behaviors=%d",
		Handle.GetId(), *GetPathNameSafe(Definition), *GetNameSafe(World), Definition->Behaviors.Num());

	if (Params.TargetEntity.IsSet())
	{
		CuesByTarget.Add(Params.TargetEntity, Handle);
	}
	if (Params.OwnerEntity.IsSet())
	{
		CuesByOwner.Add(Params.OwnerEntity, Handle);
	}

	FArcCueContext Context(Params);
	Context.Handle = Handle;
	Context.EntityManager = EntityManager;
	Context.World = World;
	Context.Definition = Definition;

	BeginCallbackDispatch();
	for (int32 Index = 0; Index < Definition->Behaviors.Num(); ++Index)
	{
		Context.BehaviorIndex = Index;
		const FInstancedStruct& BehaviorSlot = Definition->Behaviors[Index];
		const FArcCueBehavior* Behavior = BehaviorSlot.GetPtr<FArcCueBehavior>();
		if (Behavior == nullptr)
		{
			continue;
		}

		TInstancedStruct<FArcCueBehaviorState> State = Behavior->OnApplication(Context);
		if (State.IsValid())
		{
			if (ActiveCues.Contains(Handle) && Active->BehaviorStates.IsValidIndex(Index))
			{
				Active->BehaviorStates[Index] = MoveTemp(State);
			}
		}
	}
	EndCallbackDispatch();
	return Handle;
}

void UArcCueSubsystem::SendCueEvent(FArcCueHandle Handle, const FArcCueEvent& Event)
{
	check(IsInGameThread());

	if (!Handle.IsValid())
	{
		return;
	}

	if (CallbackDispatchDepth > 0)
	{
		FArcCuePendingOperation& Operation = PendingOperations.AddDefaulted_GetRef();
		Operation.Type = EArcCuePendingOperationType::CueEvent;
		Operation.Handle = Handle;
		Operation.CueEvent = Event;
		return;
	}

	BeginCallbackDispatch();
	SendCueEventImmediate(Handle, Event);
	EndCallbackDispatch();
}

void UArcCueSubsystem::SendCueStateTreeEvent(FArcCueHandle Handle, const FStateTreeEvent& Event)
{
	check(IsInGameThread());

	if (!Handle.IsValid())
	{
		UE_LOGF(LogArcCues, Warning, "[CueStateTreeEvent] Send skipped: invalid handle for event '%ls' in World=%ls",
			*Event.Tag.ToString(), *GetNameSafe(GetWorld()));
		return;
	}

	if (!ActiveCues.Contains(Handle))
	{
		UE_LOGF(LogArcCues, Warning, "[CueStateTreeEvent] Send skipped: Handle=%u removed or unknown for event '%ls' in World=%ls",
			Handle.GetId(), *Event.Tag.ToString(), *GetNameSafe(GetWorld()));
		return;
	}

	if (CallbackDispatchDepth > 0)
	{
		FArcCuePendingOperation& Operation = PendingOperations.AddDefaulted_GetRef();
		Operation.Type = EArcCuePendingOperationType::StateTreeEvent;
		Operation.Handle = Handle;
		Operation.StateTreeEvent = Event;
		return;
	}

	BeginCallbackDispatch();
	SendCueStateTreeEventImmediate(Handle, Event);
	EndCallbackDispatch();
}

void UArcCueSubsystem::RemoveCue(FArcCueHandle Handle)
{
	check(IsInGameThread());
	UE_LOGF(LogArcCues, Log, "[CueLifecycle] Remove requested: Handle=%u World=%ls Found=%d CallbackDepth=%d",
		Handle.GetId(), *GetNameSafe(GetWorld()), ActiveCues.Contains(Handle), CallbackDispatchDepth);

	if (!Handle.IsValid())
	{
		return;
	}

	if (CallbackDispatchDepth > 0)
	{
		FArcCuePendingOperation& Operation = PendingOperations.AddDefaulted_GetRef();
		Operation.Type = EArcCuePendingOperationType::Remove;
		Operation.Handle = Handle;
		return;
	}

	BeginCallbackDispatch();
	RemoveCueImmediate(Handle);
	EndCallbackDispatch();
}

void UArcCueSubsystem::SendCueEventImmediate(FArcCueHandle Handle, const FArcCueEvent& Event)
{
	UArcActiveCueInstance* Active = ActiveCues.FindRef(Handle);
	if (Active == nullptr)
	{
		return;
	}

	UArcCueDefinition* Definition = Active->Definition.Get();
	if (Definition == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = (World != nullptr) ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManager = (MassSubsystem != nullptr) ? &MassSubsystem->GetMutableEntityManager() : nullptr;

	FArcCueContext Context(Active->Parameters);
	Context.Handle = Handle;
	Context.EntityManager = EntityManager;
	Context.World = World;
	Context.Definition = Definition;

	const int32 NumBehaviors = Definition->Behaviors.Num();
	const int32 NumStates = Active->BehaviorStates.Num();
	const int32 IterCount = FMath::Min(NumBehaviors, NumStates);

	for (int32 Index = 0; Index < IterCount; ++Index)
	{
		Context.BehaviorIndex = Index;
		const FInstancedStruct& BehaviorSlot = Definition->Behaviors[Index];
		const FArcCueBehavior* Behavior = BehaviorSlot.GetPtr<FArcCueBehavior>();
		if (Behavior == nullptr)
		{
			continue;
		}

		Behavior->OnEvent(Context, Event, Active->BehaviorStates[Index]);
	}
}

void UArcCueSubsystem::SendCueStateTreeEventImmediate(FArcCueHandle Handle, const FStateTreeEvent& Event)
{
	UArcActiveCueInstance* Active = ActiveCues.FindRef(Handle);
	if (Active == nullptr)
	{
		UE_LOGF(LogArcCues, Warning, "[CueStateTreeEvent] Send skipped: Handle=%u removed or unknown for event '%ls' in World=%ls",
			Handle.GetId(), *Event.Tag.ToString(), *GetNameSafe(GetWorld()));
		return;
	}

	UArcCueDefinition* Definition = Active->Definition.Get();
	if (Definition == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = (World != nullptr) ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManager = (MassSubsystem != nullptr) ? &MassSubsystem->GetMutableEntityManager() : nullptr;

	FArcCueContext Context(Active->Parameters);
	Context.Handle = Handle;
	Context.EntityManager = EntityManager;
	Context.World = World;
	Context.Definition = Definition;

	const int32 NumBehaviors = Definition->Behaviors.Num();
	const int32 NumStates = Active->BehaviorStates.Num();
	const int32 IterCount = FMath::Min(NumBehaviors, NumStates);

	for (int32 Index = 0; Index < IterCount; ++Index)
	{
		Context.BehaviorIndex = Index;
		const FInstancedStruct& BehaviorSlot = Definition->Behaviors[Index];
		const FArcCueBehavior* Behavior = BehaviorSlot.GetPtr<FArcCueBehavior>();
		if (Behavior == nullptr)
		{
			continue;
		}

		Behavior->OnStateTreeEvent(Context, Event, Active->BehaviorStates[Index]);
	}
}

void UArcCueSubsystem::RemoveCueImmediate(FArcCueHandle Handle)
{
	UArcActiveCueInstance* Active = ActiveCues.FindRef(Handle);
	if (Active == nullptr)
	{
		UE_LOGF(LogArcCues, Warning, "[CueLifecycle] Remove skipped: Handle=%u not found in World=%ls", Handle.GetId(), *GetNameSafe(GetWorld()));
		return;
	}

	UArcCueDefinition* Definition = Active->Definition.Get();
	if (Definition != nullptr)
	{
		UWorld* World = GetWorld();
		UMassEntitySubsystem* MassSubsystem = (World != nullptr) ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
		FMassEntityManager* EntityManager = (MassSubsystem != nullptr) ? &MassSubsystem->GetMutableEntityManager() : nullptr;

		FArcCueContext Context(Active->Parameters);
		Context.Handle = Handle;
		Context.EntityManager = EntityManager;
		Context.World = World;
		Context.Definition = Definition;

		const int32 NumBehaviors = Definition->Behaviors.Num();
		const int32 NumStates = Active->BehaviorStates.Num();
		const int32 IterCount = FMath::Min(NumBehaviors, NumStates);

		for (int32 Index = IterCount - 1; Index >= 0; --Index)
		{
			Context.BehaviorIndex = Index;
			TInstancedStruct<FArcCueBehaviorState>& StateSlot = Active->BehaviorStates[Index];
			UE_LOGF(LogArcCues, Log, "[CueLifecycle] Remove behavior: Handle=%u Index=%d StateValid=%d", Handle.GetId(), Index, StateSlot.IsValid());
			if (!StateSlot.IsValid())
			{
				continue;
			}

			const FInstancedStruct& BehaviorSlot = Definition->Behaviors[Index];
			const FArcCueBehavior* Behavior = BehaviorSlot.GetPtr<FArcCueBehavior>();
			if (Behavior == nullptr)
			{
				continue;
			}

			Behavior->OnRemoved(Context, StateSlot.GetMutable());
		}
	}

	if (Active->Parameters.TargetEntity.IsSet())
	{
		CuesByTarget.RemoveSingle(Active->Parameters.TargetEntity, Handle);
	}
	if (Active->Parameters.OwnerEntity.IsSet())
	{
		CuesByOwner.RemoveSingle(Active->Parameters.OwnerEntity, Handle);
	}

	ActiveCues.Remove(Handle);
	UE_LOGF(LogArcCues, Log, "[CueLifecycle] Removed: Handle=%u World=%ls Remaining=%d", Handle.GetId(), *GetNameSafe(GetWorld()), ActiveCues.Num());
}

void UArcCueSubsystem::RemoveCuesByTag(FGameplayTag CueTag, FMassEntityHandle TargetEntity)
{
	if (!CueTag.IsValid() || !TargetEntity.IsSet())
	{
		return;
	}

	TArray<FArcCueHandle> ToRemove;
	for (TMultiMap<FMassEntityHandle, FArcCueHandle>::TConstKeyIterator It(CuesByTarget, TargetEntity); It; ++It)
	{
		const FArcCueHandle Handle = It.Value();
		if (const UArcActiveCueInstance* Active = ActiveCues.FindRef(Handle))
		{
			const UArcCueDefinition* Definition = Active->Definition.Get();
			if (Definition != nullptr && Definition->CueTag == CueTag)
			{
				ToRemove.Add(Handle);
			}
		}
	}

	for (const FArcCueHandle& Handle : ToRemove)
	{
		RemoveCue(Handle);
	}
}

void UArcCueSubsystem::RemoveAllCues(FMassEntityHandle TargetEntity)
{
	if (!TargetEntity.IsSet())
	{
		return;
	}

	TArray<FArcCueHandle> ToRemove;
	for (TMultiMap<FMassEntityHandle, FArcCueHandle>::TConstKeyIterator It(CuesByTarget, TargetEntity); It; ++It)
	{
		ToRemove.Add(It.Value());
	}

	for (const FArcCueHandle& Handle : ToRemove)
	{
		RemoveCue(Handle);
	}
}

void UArcCueSubsystem::RemoveCuesByOwner(FMassEntityHandle OwnerEntity)
{
	if (!OwnerEntity.IsSet())
	{
		return;
	}

	TArray<FArcCueHandle> ToRemove;
	for (TMultiMap<FMassEntityHandle, FArcCueHandle>::TConstKeyIterator It(CuesByOwner, OwnerEntity); It; ++It)
	{
		ToRemove.Add(It.Value());
	}

	for (const FArcCueHandle& Handle : ToRemove)
	{
		RemoveCue(Handle);
	}
}

void UArcCueSubsystem::OnCueAttachmentLost(FArcCueHandle Handle, int32 BehaviorIndex, FMassEntityHandle AttachmentEntity)
{
	check(IsInGameThread());

	UArcActiveCueInstance* Active = ActiveCues.FindRef(Handle);
	if (Active == nullptr || !Active->BehaviorStates.IsValidIndex(BehaviorIndex))
	{
		return;
	}

	UArcCueDefinition* Definition = Active->Definition.Get();
	if (Definition == nullptr || !Definition->Behaviors.IsValidIndex(BehaviorIndex))
	{
		return;
	}

	TInstancedStruct<FArcCueBehaviorState>& State = Active->BehaviorStates[BehaviorIndex];
	if (!State.IsValid())
	{
		return;
	}

	const FArcCueBehavior* Behavior = Definition->Behaviors[BehaviorIndex].GetPtr<FArcCueBehavior>();
	if (Behavior == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = (World != nullptr) ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManager = (MassSubsystem != nullptr) ? &MassSubsystem->GetMutableEntityManager() : nullptr;

	FArcCueContext Context(Active->Parameters);
	Context.Handle = Handle;
	Context.EntityManager = EntityManager;
	Context.World = World;
	Context.Definition = Definition;
	Context.BehaviorIndex = BehaviorIndex;

	BeginCallbackDispatch();
	Behavior->OnAttachmentLost(Context, AttachmentEntity, State);
	EndCallbackDispatch();
}

void UArcCueSubsystem::QueueCueEntityDestroy(FMassEntityHandle Entity)
{
	if (Entity.IsValid())
	{
		PendingEntityDestroys.AddUnique(Entity);
	}
}

void UArcCueSubsystem::RegisterDefinitionForTests(UArcCueDefinition* Definition)
{
	if (Definition == nullptr || !Definition->CueTag.IsValid())
	{
		return;
	}
	TagToDefinitions.FindOrAdd(Definition->CueTag).AddUnique(TSoftObjectPtr<UArcCueDefinition>(Definition));
}

void UArcCueSubsystem::UnregisterDefinitionForTests(UArcCueDefinition* Definition)
{
	if (Definition == nullptr)
	{
		return;
	}
	const TSoftObjectPtr<UArcCueDefinition> SoftRef(Definition);
	for (TPair<FGameplayTag, TArray<TSoftObjectPtr<UArcCueDefinition>>>& Pair : TagToDefinitions)
	{
		Pair.Value.Remove(SoftRef);
	}
}

void UArcCueSubsystem::RebuildDiscoveryCache()
{
	TagToDefinitions.Reset();

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (AssetRegistry == nullptr)
	{
		return;
	}

	TArray<FAssetData> Found;
	AssetRegistry->GetAssetsByClass(UArcCueDefinition::StaticClass()->GetClassPathName(), Found, /*bSearchSubClasses=*/true);

	for (const FAssetData& Data : Found)
	{
		const FGameplayTag Tag = UArcCueDefinition::GetCueTagFromAssetData(Data);
		if (!Tag.IsValid())
		{
			continue;
		}
		TagToDefinitions.FindOrAdd(Tag).Add(TSoftObjectPtr<UArcCueDefinition>(Data.GetSoftObjectPath()));
	}
}

void UArcCueSubsystem::OnAssetAdded(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath != UArcCueDefinition::StaticClass()->GetClassPathName())
	{
		return;
	}

	const FGameplayTag Tag = UArcCueDefinition::GetCueTagFromAssetData(AssetData);
	if (!Tag.IsValid())
	{
		return;
	}
	TagToDefinitions.FindOrAdd(Tag).AddUnique(TSoftObjectPtr<UArcCueDefinition>(AssetData.GetSoftObjectPath()));
}

void UArcCueSubsystem::OnAssetRemoved(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath != UArcCueDefinition::StaticClass()->GetClassPathName())
	{
		return;
	}

	const TSoftObjectPtr<UArcCueDefinition> SoftRef(AssetData.GetSoftObjectPath());
	for (TPair<FGameplayTag, TArray<TSoftObjectPtr<UArcCueDefinition>>>& Pair : TagToDefinitions)
	{
		Pair.Value.Remove(SoftRef);
	}
}

void UArcCueSubsystem::GatherMatchingDefinitions(FGameplayTag CueTag, const FGameplayTagContainer& ContextTags, TArray<UArcCueDefinition*>& OutDefinitions)
{
	const TArray<TSoftObjectPtr<UArcCueDefinition>>* SoftRefs = TagToDefinitions.Find(CueTag);
	if (SoftRefs == nullptr)
	{
		return;
	}

	for (const TSoftObjectPtr<UArcCueDefinition>& SoftRef : *SoftRefs)
	{
		// Sync load on first fire — cached by the soft ptr afterwards.
		UArcCueDefinition* Definition = SoftRef.LoadSynchronous();
		if (Definition == nullptr)
		{
			continue;
		}

		// FilterTags semantics: a definition matches if either it has no FilterTags
		// (wildcard) or every filter tag is present in ContextTags. Mirrors GAS-style
		// secondary filtering on cues without going through the engine cue manager.
		if (Definition->FilterTags.Num() > 0 && !ContextTags.HasAll(Definition->FilterTags))
		{
			continue;
		}

		OutDefinitions.Add(Definition);
	}
}

FArcCueHandle UArcCueSubsystem::AllocateHandle()
{
	const uint32 Id = NextHandleId++;
	if (NextHandleId == 0)
	{
		// Skip zero — reserved for Invalid().
		NextHandleId = 1;
	}
	return FArcCueHandle(Id);
}

void UArcCueSubsystem::BeginCallbackDispatch()
{
	++CallbackDispatchDepth;
}

void UArcCueSubsystem::EndCallbackDispatch()
{
	check(CallbackDispatchDepth > 0);
	--CallbackDispatchDepth;
	if (CallbackDispatchDepth == 0 && !bFlushingPendingOperations)
	{
		FlushPendingOperations();
	}
}

void UArcCueSubsystem::FlushPendingOperations()
{
	if (CallbackDispatchDepth > 0 || bFlushingPendingOperations)
	{
		return;
	}

	bFlushingPendingOperations = true;
	for (int32 Index = 0; Index < PendingOperations.Num(); ++Index)
	{
		FArcCuePendingOperation Operation = PendingOperations[Index];
		BeginCallbackDispatch();
		if (Operation.Type == EArcCuePendingOperationType::CueEvent)
		{
			SendCueEventImmediate(Operation.Handle, Operation.CueEvent);
		}
		else if (Operation.Type == EArcCuePendingOperationType::StateTreeEvent)
		{
			SendCueStateTreeEventImmediate(Operation.Handle, Operation.StateTreeEvent);
		}
		else
		{
			RemoveCueImmediate(Operation.Handle);
		}
		EndCallbackDispatch();
	}
	PendingOperations.Reset();
	bFlushingPendingOperations = false;
}

void UArcCueSubsystem::FlushPendingEntityDestroys(FMassEntityManager* EntityManager)
{
	if (EntityManager == nullptr || !EntityManager->CanMutateSynchronously())
	{
		return;
	}

	TArray<FMassEntityHandle> Entities = MoveTemp(PendingEntityDestroys);
	PendingEntityDestroys.Reset();

	for (const FMassEntityHandle& Entity : Entities)
	{
		if (!EntityManager->IsEntityValid(Entity))
		{
			continue;
		}
		if (!EntityManager->IsEntityActive(Entity))
		{
			PendingEntityDestroys.AddUnique(Entity);
			continue;
		}

		EntityManager->DestroyEntity(Entity);
	}
}
