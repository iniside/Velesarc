// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcTacticsInstanceSubsystem.h"

#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "ArcTQSQueryInstance.h"
#include "ArcTQSQuerySubsystem.h"
#include "ArcTacticsModule.h"
#include "ArcTacticsSignals.h"
#include "Debugger/ArcTacticsTrace.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsDecision.h"
#include "Engine/World.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/ArcTacticsRequirements.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityUtils.h"
#include "Mass/EntityView.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassWorldUtils.h"
#include "Navigation/ArcPathFollow.h"
#include "Teams/ArcMassTeamFragment.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsInstanceSubsystem)

UE_DEFINE_MASS_SUBSYSTEM_WRAPPER(UArcTacticsInstanceSubsystem, FMassSystem_ArcTacticsInstanceSubsystem);

namespace ArcTacticsInstanceSubsystemPrivate
{
	uint32 AdvanceNonZero(uint32& Value)
	{
		++Value;
		if (Value == 0)
		{
			++Value;
		}
		return Value;
	}

	FArcTacticsTaskContext MakeTaskContext(
		UArcTacticsInstanceSubsystem& Subsystem,
		FMassEntityManager& EntityManager,
		FArcTacticsInstanceSlot& Slot,
		const FArcTacticsInstanceHandle Handle)
	{
		FArcTacticsTaskContext Context;
		Context.World = Subsystem.GetWorld();
		Context.EntityManager = &EntityManager;
		Context.Entity = Slot.Entity;
		Context.InstanceSubsystem = &Subsystem;
		Context.InstanceHandle = Handle;
		Context.AsyncCompletion = FArcTacticsTaskAsyncCompletion::CreateUObject(
			&Subsystem, &UArcTacticsInstanceSubsystem::CompleteTaskQuery);
		Context.DecisionGeneration = Slot.DecisionGeneration;
		return Context;
	}

	TOptional<ArcPathFollow::FContext> MakePathContext(
		UWorld& World,
		FMassEntityManager& EntityManager,
		const FMassEntityHandle Entity)
	{
		if (!EntityManager.IsEntityActive(Entity))
		{
			return TOptional<ArcPathFollow::FContext>();
		}
		const FMassEntityView EntityView(EntityManager, Entity);
		FTransformFragment* Transform = EntityView.GetFragmentDataPtr<FTransformFragment>();
		const FAgentRadiusFragment* Radius = EntityView.GetFragmentDataPtr<FAgentRadiusFragment>();
		const FAgentHeightFragment* Height = EntityView.GetFragmentDataPtr<FAgentHeightFragment>();
		const FMassDesiredMovementFragment* Desired = EntityView.GetFragmentDataPtr<FMassDesiredMovementFragment>();
		FMassMoveTargetFragment* MoveTarget = EntityView.GetFragmentDataPtr<FMassMoveTargetFragment>();
		FMassNavMeshShortPathFragment* ShortPath = EntityView.GetFragmentDataPtr<FMassNavMeshShortPathFragment>();
		FMassNavMeshCachedPathFragment* CachedPath = EntityView.GetFragmentDataPtr<FMassNavMeshCachedPathFragment>();
		const FMassMovementParameters* Movement =
			EntityView.GetConstSharedFragmentDataPtr<FMassMovementParameters>();
		const FArcMassTeamFragment* Team = EntityView.GetFragmentDataPtr<FArcMassTeamFragment>();
		if (!Transform || !Radius || !Height || !Desired || !MoveTarget || !ShortPath || !CachedPath || !Movement)
		{
			return TOptional<ArcPathFollow::FContext>();
		}
		return TOptional<ArcPathFollow::FContext>(ArcPathFollow::FContext{
			World, Entity, *Transform, *Radius, *Height, *Desired, *Movement,
			*MoveTarget, *ShortPath, *CachedPath, Team ? Team->TeamId : INDEX_NONE});
	}

	FString JoinNames(const TConstArrayView<FName> Names)
	{
		FString Result;
		for (const FName Name : Names)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			Result += Name.ToString();
		}
		return Result;
	}
}

bool FArcTacticsInstanceSlot::Initialize(
	const FMassEntityHandle InEntity,
	const FArcTacticsDecisionSettings& InSettings,
	const int32 RandomSeed)
{
	Entity = InEntity;
	Context = FArcTacticsContextData();
	Context.Self = InEntity;
	Settings = FArcTacticsDecisionSettings();
	Packages.Reset();
	DecisionGeneration = 0;
	DecisionStatus = EArcTacticsDecisionStatus::Idle;
	PackageCursor = INDEX_NONE;
	Candidates.Reset();
	SelectedAction.Reset();
	ActiveAction.Reset();
	LastEndedAction = FArcTacticsEndedAction();
	CooldownEndTimes.Reset();
	Diagnostic = NAME_None;
	if (!InSettings.IsValid())
	{
		Diagnostic = TEXT("InvalidTraitDecisionSettings");
		DecisionStatus = EArcTacticsDecisionStatus::Failed;
		return false;
	}
	Settings = InSettings;
	RandomStream.Initialize(RandomSeed);
	return true;
}

FArcTacticsPackage& FArcTacticsInstanceSlot::AddPackage(
	const UArcTacticsConfig* Config,
	TSharedPtr<const FArcTacticsCompiledData> InDefinitions)
{
	FArcTacticsPackage& Package = *Packages.Add_GetRef(MakeUnique<FArcTacticsPackage>());
	Package.Config = Config;
	Package.SeenRevision = Config ? Config->CompiledData.Revision : 0;
	Package.Serial = ArcTacticsInstanceSubsystemPrivate::AdvanceNonZero(NextPackageSerial);
	if (InDefinitions && Package.Instance.Initialize(*InDefinitions, &Context))
	{
		Package.Definitions = MoveTemp(InDefinitions);
		Package.bActive = true;
	}
	else
	{
		Package.Instance.Reset();
	}
	return Package;
}

int32 FArcTacticsInstanceSlot::FindPackageIndex(const UArcTacticsConfig& Config) const
{
	return Packages.IndexOfByPredicate([&Config](const TUniquePtr<FArcTacticsPackage>& Package)
	{
		return Package && Package->Config == &Config;
	});
}

FArcTacticsPackage* FArcTacticsInstanceSlot::GetActionPackage(const FArcTacticsOption& Option)
{
	FArcTacticsPackage* Package = Packages.IsValidIndex(Option.PackageIndex)
		? Packages[Option.PackageIndex].Get()
		: nullptr;
	return Package && Package->Serial == Option.PackageSerial && Package->bActive && Package->Definitions
		? Package
		: nullptr;
}

bool FArcTacticsInstanceSlot::HasActivePackage() const
{
	return Packages.ContainsByPredicate([](const TUniquePtr<FArcTacticsPackage>& Package)
	{
		return Package && Package->bActive && Package->Definitions;
	});
}

bool FArcTacticsInstanceSlot::ResetForDecision(const FArcTQSFunctionContext& FunctionContext)
{
	ArcTacticsInstanceSubsystemPrivate::AdvanceNonZero(DecisionGeneration);
	DecisionStatus = EArcTacticsDecisionStatus::Running;
	PackageCursor = 0;
	Candidates.Reset();
	SelectedAction.Reset();
	Diagnostic = NAME_None;
	bool bAnyIncluded = false;
	FName FirstExclusion;
	for (const TUniquePtr<FArcTacticsPackage>& PackagePtr : Packages)
	{
		FArcTacticsPackage& Package = *PackagePtr;
		Package.bExcluded = true;
		Package.Instance.bGlobalTaskRunning = false;
		if (!Package.bActive || !Package.Definitions)
		{
			continue;
		}
		if (!Package.Instance.IsCompatible(*Package.Definitions))
		{
			Package.Instance.Diagnostic = TEXT("IncompatibleCompiledData");
		}
		else if (Package.Instance.ResetForDecision(*Package.Definitions, FunctionContext))
		{
			Package.bExcluded = false;
			bAnyIncluded = true;
			continue;
		}
		if (FirstExclusion.IsNone())
		{
			FirstExclusion = Package.Instance.Diagnostic;
		}
	}
	if (!bAnyIncluded)
	{
		Diagnostic = FirstExclusion;
		DecisionStatus = EArcTacticsDecisionStatus::Failed;
	}
	return bAnyIncluded;
}

bool FArcTacticsInstanceSlot::HasSelectedAction() const
{
	return SelectedAction.IsSet();
}

void FArcTacticsInstanceSlot::ActivateSelectedAction()
{
	ActiveAction = SelectedAction;
	ActiveAction.Serial = ArcTacticsInstanceSubsystemPrivate::AdvanceNonZero(NextActionSerial);
	SelectedAction.Reset();
}

void FArcTacticsInstanceSlot::ClearActiveAction()
{
	ActiveAction.Reset();
}

void FArcTacticsInstanceSlot::AddReferencedObjects(FReferenceCollector& Collector, const UObject* Referencer)
{
	for (const TUniquePtr<FArcTacticsPackage>& PackagePtr : Packages)
	{
		if (!PackagePtr)
		{
			continue;
		}
		FArcTacticsPackage& Package = *PackagePtr;
		Collector.AddReferencedObject(Package.Config, Referencer);
		Package.Instance.AddReferencedObjects(Collector);
		if (Package.Definitions)
		{
			Collector.AddPropertyReferencesWithStructARO(
				FArcTacticsCompiledData::StaticStruct(),
				const_cast<FArcTacticsCompiledData*>(Package.Definitions.Get()),
				Referencer);
		}
	}
	for (FArcTacticsPendingPackageChange& Change : PendingPackageChanges)
	{
		Collector.AddReferencedObject(Change.Config, Referencer);
	}
	for (FArcTacticsCandidate& Candidate : Candidates)
	{
		Candidate.AddReferencedObjects(Collector);
	}
	SelectedAction.AddReferencedObjects(Collector);
	ActiveAction.AddReferencedObjects(Collector);
}

FArcTacticsExecutionScope::FArcTacticsExecutionScope(
	UArcTacticsInstanceSubsystem& InSubsystem,
	const FArcTacticsInstanceHandle InHandle)
	: Subsystem(&InSubsystem)
	, Handle(InHandle)
{
	if (FArcTacticsInstanceSlot* Slot = InSubsystem.Resolve(Handle))
	{
		++Slot->ExecutionDepth;
		bEntered = true;
	}
}

FArcTacticsExecutionScope::~FArcTacticsExecutionScope()
{
	UArcTacticsInstanceSubsystem* Instances = Subsystem.Get();
	if (bEntered && Instances)
	{
		Instances->EndExecution(Handle);
	}
}

FArcTacticsPackage* ArcTacticsInstances::ResolveActivePackage(FArcTacticsInstanceSlot& Slot)
{
	return Slot.ActiveAction.IsSet() ? Slot.GetActionPackage(Slot.ActiveAction) : nullptr;
}

const FArcTacticsActionNode* ArcTacticsInstances::ResolveActiveAction(
	FArcTacticsInstanceSlot& Slot,
	FArcTacticsActionView& OutAction)
{
	const FArcTacticsPackage* Package = ResolveActivePackage(Slot);
	if (!Package)
	{
		return nullptr;
	}
	const int32 NodeIndex = Slot.ActiveAction.Action.Get();
	if (!Package->Definitions->Nodes.IsValidIndex(NodeIndex))
	{
		return nullptr;
	}
	OutAction = FArcTacticsActionView(*Package->Definitions, Slot.ActiveAction);
	return Package->Definitions->Nodes[NodeIndex].Node.GetPtr<FArcTacticsActionNode>();
}

FArcTacticsActionContext ArcTacticsInstances::MakeActionContext(
	UArcTacticsInstanceSubsystem& Subsystem,
	FMassEntityManager& EntityManager,
	FArcTacticsInstanceSlot& Slot,
	const FArcTacticsInstanceHandle Handle,
	const ArcPathFollow::FContext* PathContext)
{
	UWorld* World = Subsystem.GetWorld();
	FArcTacticsActionContext Context;
	Context.World = World;
	Context.EntityManager = &EntityManager;
	Context.QuerySubsystem = World ? World->GetSubsystem<UArcTQSQuerySubsystem>() : nullptr;
	Context.AbilitySubsystem = World ? World->GetSubsystem<UArcAbilityStateTreeSubsystem>() : nullptr;
	Context.PathContext = PathContext;
	Context.HoldState = &Slot.MovementHold;
	Context.Entity = Slot.Entity;
	Context.RequestToken.InstanceSubsystem = &Subsystem;
	Context.RequestToken.Entity = Slot.Entity;
	Context.RequestToken.InstanceHandle = Handle;
	Context.RequestToken.DecisionGeneration = Slot.DecisionGeneration;
	Context.RequestToken.ActionSerial = Slot.ActiveAction.Serial;
	Context.AsyncCompletion = FArcTacticsActionAsyncCompletion::CreateUObject(
		&Subsystem, &UArcTacticsInstanceSubsystem::CompleteActionQuery);
	Context.RequestExecution = FSimpleDelegate::CreateWeakLambda(
		&Subsystem,
		[WeakSubsystem = TWeakObjectPtr<UArcTacticsInstanceSubsystem>(&Subsystem),
		 Handle, Entity = Slot.Entity, Serial = Slot.ActiveAction.Serial]()
		{
			UArcTacticsInstanceSubsystem* InstanceSubsystem = WeakSubsystem.Get();
			FArcTacticsInstanceSlot* ResolvedSlot = InstanceSubsystem
				? InstanceSubsystem->Resolve(Handle) : nullptr;
			if (!ResolvedSlot || ResolvedSlot->Entity != Entity
				|| !ResolvedSlot->ActiveAction.IsSet()
				|| ResolvedSlot->ActiveAction.Serial != Serial)
			{
				return;
			}
			UWorld* RequestWorld = InstanceSubsystem->GetWorld();
			UMassSignalSubsystem* Signals =
				RequestWorld ? RequestWorld->GetSubsystem<UMassSignalSubsystem>() : nullptr;
			if (Signals)
			{
				ResolvedSlot->ActionWakeSerial = Serial;
				ResolvedSlot->ActionWakeTime = RequestWorld->GetTimeSeconds();
				Signals->SignalEntity(UE::ArcTactics::Signals::ActionWakeup, Entity);
			}
		});
	if (FArcTacticsPackage* Package = ResolveActivePackage(Slot))
	{
		FArcTQSFunctionContext FunctionContext;
		FunctionContext.EntityManager = &EntityManager;
		FunctionContext.World = World;
		FunctionContext.Self = Slot.Entity;
		Context.Steps = FArcTacticsStepAccess(
			*Package->Definitions, Slot.ActiveAction, &Package->Instance, FunctionContext);
		Context.ActiveSteps = FArcTacticsActiveStepAccess(
			*Package->Definitions, Slot.ActiveAction, &Package->Instance, FunctionContext);
	}
	return Context;
}

void ArcTacticsInstances::StampCooldown(
	FArcTacticsInstanceSlot& Slot,
	const FArcTacticsPackage& Package,
	UWorld& World)
{
	if (!Package.Definitions)
	{
		return;
	}
	const float Cooldown = Slot.ActiveAction.Parameters.Cooldown;
	Slot.CooldownEndTimes.Add(
		FArcTacticsCooldownKey(
			Package.Config.Get(),
			FArcTacticsInstanceData::GetBranchIndex(*Package.Definitions, Slot.ActiveAction)),
		World.GetTimeSeconds() + Cooldown);
	UMassSignalSubsystem* Signals = World.GetSubsystem<UMassSignalSubsystem>();
	if (Signals && Cooldown > 0.0f)
	{
		Signals->DelaySignalEntity(UE::ArcTactics::Signals::DecisionDue, Slot.Entity, Cooldown);
	}
}

void ArcTacticsInstances::RecordLastEndedAction(
	FArcTacticsInstanceSlot& Slot,
	const UWorld* World,
	const EArcTacticsActionEndStatus EndStatus,
	const EArcTacticsActionEndReason EndReason,
	const FName FailureReason)
{
	if (!Slot.ActiveAction.IsSet())
	{
		return;
	}
	const int32 PackageIndex = Slot.ActiveAction.PackageIndex;
	const UArcTacticsConfig* Config = Slot.Packages.IsValidIndex(PackageIndex) && Slot.Packages[PackageIndex]
		? Slot.Packages[PackageIndex]->Config.Get()
		: nullptr;
	FArcTacticsEndedAction& Ended = Slot.LastEndedAction;
	Ended.bSet = true;
	Ended.ConfigName = Config ? Config->GetFName() : NAME_None;
	Ended.Name = Slot.ActiveAction.Name;
	Ended.Status = EndStatus;
	Ended.Reason = EndReason;
	Ended.FailureReason = FailureReason;
	Ended.Time = World ? World->GetTimeSeconds() : 0.0;
}

void ArcTacticsInstances::RecordEndedAction(
	FArcTacticsInstanceSlot& Slot,
	const FArcTacticsPackage& Package,
	UWorld& World,
	const FMassEntityManager& EntityManager,
	const EArcTacticsActionEndStatus EndStatus,
	const EArcTacticsActionEndReason EndReason,
	const FName FailureReason)
{
	if (!Slot.ActiveAction.IsSet())
	{
		return;
	}
	const double Now = World.GetTimeSeconds();
	if (Slot.ActiveAction.Parameters.CooldownStart == EArcTacticsCooldownStart::OnEnd)
	{
		StampCooldown(Slot, Package, World);
	}
	const FGameplayTag HistoryTag = Slot.ActiveAction.Parameters.HistoryTag;
	const bool bAppendHistory = EndStatus == EArcTacticsActionEndStatus::Succeeded && HistoryTag.IsValid();
	if (bAppendHistory)
	{
		TArray<FArcTacticsActionHistoryEntry>& Entries = Slot.Context.History.Entries;
		FArcTacticsActionHistoryEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Tag = HistoryTag;
		Entry.Time = Now;
		const int32 HistorySize = FMath::Max(1, Slot.Settings.HistorySize);
		if (Entries.Num() > HistorySize)
		{
			Entries.RemoveAt(0, Entries.Num() - HistorySize);
		}
	}
	RecordLastEndedAction(Slot, &World, EndStatus, EndReason, FailureReason);
	TRACE_ARCTACTICS_ACTION_ENDED(&World, &EntityManager, Slot, EndStatus, EndReason,
		Slot.ActiveAction.Parameters.CooldownStart == EArcTacticsCooldownStart::OnEnd, bAppendHistory);
}

void UArcTacticsInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassEntitySubsystem>();
	Collection.InitializeDependency<UArcTQSQuerySubsystem>();
	Collection.InitializeDependency<UArcAbilityStateTreeSubsystem>();
	Collection.InitializeDependency<UMassSignalSubsystem>();
	Collection.InitializeDependency<UArcGameEventSubsystem>();
	SignalSubsystem = GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	GameEventSubsystem = GetWorld()->GetSubsystem<UArcGameEventSubsystem>();
}

void UArcTacticsInstanceSubsystem::PreDeinitialize()
{
	UWorld* World = GetWorld();
	FMassEntityManager* EntityManager = World ? UE::Mass::Utils::GetEntityManager(World) : nullptr;
	if (World && EntityManager)
	{
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			FArcTacticsInstanceSlot& Slot = *Slots[SlotIndex];
			if (!Slot.bAllocated)
			{
				continue;
			}
			const TOptional<ArcPathFollow::FContext> PathContext =
				ArcTacticsInstanceSubsystemPrivate::MakePathContext(*World, *EntityManager, Slot.Entity);
			CancelSlot(
				*EntityManager, Slot, FArcTacticsInstanceHandle{SlotIndex, Slot.Generation},
				PathContext.GetPtrOrNull(), EArcTacticsActionCancelMode::ReleaseHold);
		}
	}
	Super::PreDeinitialize();
}

void UArcTacticsInstanceSubsystem::Deinitialize()
{
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (Slots[SlotIndex] && Slots[SlotIndex]->bAllocated)
		{
			FreeSlot(FArcTacticsInstanceHandle{SlotIndex, Slots[SlotIndex]->Generation});
		}
	}
	CurrentDefinitions.Reset();
	Slots.Reset();
	Freelist.Reset();
	GameEventSubsystem.Reset();
	SignalSubsystem.Reset();
	Super::Deinitialize();
}

TSharedPtr<const FArcTacticsCompiledData> UArcTacticsInstanceSubsystem::ResolveDefinitions(
	const UArcTacticsConfig& Config)
{
	const TWeakObjectPtr<const UArcTacticsConfig> ConfigKey(&Config);
	if (const TWeakPtr<const FArcTacticsCompiledData>* ExistingWeak = CurrentDefinitions.Find(ConfigKey))
	{
		const TSharedPtr<const FArcTacticsCompiledData> Existing = ExistingWeak->Pin();
		if (Existing && Existing->Revision == Config.CompiledData.Revision)
		{
			return Existing;
		}
	}
	const TSharedPtr<const FArcTacticsCompiledData> Definitions =
		MakeShared<FArcTacticsCompiledData>(Config.CompiledData);
	CurrentDefinitions.FindOrAdd(ConfigKey) = Definitions;
	return Definitions;
}

FArcTacticsPackage& UArcTacticsInstanceSubsystem::BuildPackage(
	FMassEntityManager& EntityManager,
	FArcTacticsInstanceSlot& Slot,
	const UArcTacticsConfig& Config)
{
	TSharedPtr<const FArcTacticsCompiledData> Definitions;
	if (!Config.CompiledData.IsExecutable())
	{
		UE_LOGF(LogArcTactics, Error, "ArcTactics: config %ls on entity %ls is not executable; package not activated.",
			*Config.GetName(), *Slot.Entity.DebugGetDescription());
	}
	else
	{
		TArray<FName> Missing;
		if (ArcTacticsRequirements::FindMissing(
			EntityManager, Slot.Entity, ArcTacticsRequirements::Gather(Config.CompiledData), Missing))
		{
			UE_LOGF(LogArcTactics, Error, "ArcTactics: config %ls on entity %ls needs %ls; package not activated.",
				*Config.GetName(), *Slot.Entity.DebugGetDescription(),
				*ArcTacticsInstanceSubsystemPrivate::JoinNames(Missing));
		}
		else
		{
			Definitions = ResolveDefinitions(Config);
		}
	}
	return Slot.AddPackage(&Config, MoveTemp(Definitions));
}

FArcTacticsInstanceHandle UArcTacticsInstanceSubsystem::AllocateInstance(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const TConstArrayView<TObjectPtr<const UArcTacticsConfig>> Configs,
	const FArcTacticsDecisionSettings& Settings)
{
	if (!Entity.IsSet())
	{
		return FArcTacticsInstanceHandle();
	}
	const int32 SlotIndex = Freelist.IsEmpty()
		? Slots.Add(MakeUnique<FArcTacticsInstanceSlot>())
		: Freelist.Pop(EAllowShrinking::No);
	FArcTacticsInstanceSlot& Slot = *Slots[SlotIndex];
	if (Slot.bRetired || Slot.Generation == 0)
	{
		return FArcTacticsInstanceHandle();
	}
	if (!Slot.Initialize(Entity, Settings, HashCombine(GetTypeHash(Entity), Slot.Generation)))
	{
		UE_LOGF(LogArcTactics, Error, "ArcTactics: entity %ls has invalid decision settings; instance not allocated.",
			*Entity.DebugGetDescription());
		const uint32 Generation = Slot.Generation;
		Slot = FArcTacticsInstanceSlot();
		Slot.Generation = Generation;
		Freelist.Add(SlotIndex);
		return FArcTacticsInstanceHandle();
	}
	for (const TObjectPtr<const UArcTacticsConfig>& Config : Configs)
	{
		if (!Config)
		{
			continue;
		}
		const int32 PackageIndex = Slot.FindPackageIndex(*Config);
		if (PackageIndex != INDEX_NONE)
		{
			++Slot.Packages[PackageIndex]->StaticCount;
			continue;
		}
		BuildPackage(EntityManager, Slot, *Config).StaticCount = 1;
	}
	Slot.MovementHold = FArcTacticsMovementHoldState();
	Slot.NextDecisionTime = 0.0;
	Slot.ScheduledDecisionTime = 0.0;
	Slot.ActionWakeTime = 0.0;
	Slot.ActionWakeSerial = 0;
	Slot.bAllocated = true;
	Slot.bDecisionRequested = false;
	Slot.bActionEntered = false;
	if (UArcGameEventSubsystem* Events = GameEventSubsystem.Get())
	{
		Slot.AbilityActivatedHandle = Events->Subscribe(
			ArcGameEventTags::AbilityActivated, Entity, EArcEventRole::Target,
			FArcGameEventDelegate::CreateUObject(this, &UArcTacticsInstanceSubsystem::HandleAbilityActivated));
		Slot.AbilityEndedHandle = Events->Subscribe(
			ArcGameEventTags::AbilityEnded, Entity, EArcEventRole::Target,
			FArcGameEventDelegate::CreateUObject(this, &UArcTacticsInstanceSubsystem::HandleAbilityEnded));
		Slot.AbilityCooldownExpiredHandle = Events->Subscribe(
			ArcGameEventTags::AbilityCooldownExpired, Entity, EArcEventRole::Target,
			FArcGameEventDelegate::CreateUObject(this, &UArcTacticsInstanceSubsystem::HandleAbilityCooldownExpired));
	}
	return FArcTacticsInstanceHandle{SlotIndex, Slot.Generation};
}

FArcTacticsInstanceSlot* UArcTacticsInstanceSubsystem::Resolve(const FArcTacticsInstanceHandle Handle)
{
	return Slots.IsValidIndex(Handle.Index)
		&& Slots[Handle.Index]
		&& Slots[Handle.Index]->bAllocated
		&& Slots[Handle.Index]->Generation == Handle.Generation
		? Slots[Handle.Index].Get()
		: nullptr;
}

const FArcTacticsInstanceSlot* UArcTacticsInstanceSubsystem::Resolve(
	const FArcTacticsInstanceHandle Handle) const
{
	return const_cast<UArcTacticsInstanceSubsystem*>(this)->Resolve(Handle);
}

bool UArcTacticsInstanceSubsystem::IsCurrentRevision(const FArcTacticsInstanceHandle Handle) const
{
	const FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	if (!Slot)
	{
		return false;
	}
	for (const TUniquePtr<FArcTacticsPackage>& Package : Slot->Packages)
	{
		if (!Package->Config || Package->SeenRevision != Package->Config->CompiledData.Revision)
		{
			return false;
		}
	}
	return true;
}

FArcTacticsInstanceHandle UArcTacticsInstanceSubsystem::FindHandle(const FMassEntityHandle Entity) const
{
	UWorld* World = GetWorld();
	FMassEntityManager* EntityManager = World ? UE::Mass::Utils::GetEntityManager(World) : nullptr;
	if (!EntityManager || !EntityManager->IsEntityActive(Entity))
	{
		return FArcTacticsInstanceHandle();
	}
	const FArcTacticsInstanceFragment* InstanceFragment =
		EntityManager->GetFragmentDataPtr<FArcTacticsInstanceFragment>(Entity);
	const FArcTacticsInstanceSlot* Slot = InstanceFragment ? Resolve(InstanceFragment->Handle) : nullptr;
	return Slot && Slot->Entity == Entity ? InstanceFragment->Handle : FArcTacticsInstanceHandle();
}

void UArcTacticsInstanceSubsystem::RequestDecision(const FMassEntityHandle Entity)
{
	check(IsInGameThread());
	const FArcTacticsInstanceHandle Handle = FindHandle(Entity);
	if (Handle.IsValid())
	{
		RequestDecision(Handle);
	}
}

void UArcTacticsInstanceSubsystem::RequestDecision(const FArcTacticsInstanceHandle Handle)
{
	FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	if (!Slot || !Slot->HasActivePackage())
	{
		return;
	}
	Slot->bDecisionRequested = true;
	SignalEntity(UE::ArcTactics::Signals::DecisionDue, Slot->Entity);
}

void UArcTacticsInstanceSubsystem::SignalEntity(const FName Signal, const FMassEntityHandle Entity) const
{
	if (UMassSignalSubsystem* Signals = SignalSubsystem.Get())
	{
		Signals->SignalEntity(Signal, Entity);
	}
}

void UArcTacticsInstanceSubsystem::EndExecution(const FArcTacticsInstanceHandle Handle)
{
	FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	if (!Slot)
	{
		return;
	}
	if (Slot->ExecutionDepth > 1)
	{
		--Slot->ExecutionDepth;
		return;
	}
	for (int32 ChangeIndex = 0; Slot && ChangeIndex < Slot->PendingPackageChanges.Num(); ++ChangeIndex)
	{
		const FArcTacticsPendingPackageChange Change = Slot->PendingPackageChanges[ChangeIndex];
		Slot->PendingApplyIndex = ChangeIndex + 1;
		if (Change.Config)
		{
			if (Change.bAdd)
			{
				ApplyAddConfig(Handle, *Change.Config);
			}
			else
			{
				ApplyRemoveConfig(Handle, *Change.Config);
			}
		}
		Slot = Resolve(Handle);
	}
	if (Slot)
	{
		Slot->PendingPackageChanges.Reset();
		Slot->PendingApplyIndex = 0;
		Slot->ExecutionDepth = 0;
	}
}

bool UArcTacticsInstanceSubsystem::AddConfig(const FMassEntityHandle Entity, const UArcTacticsConfig& Config)
{
	check(IsInGameThread());
	const FArcTacticsInstanceHandle Handle = FindHandle(Entity);
	FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	if (!Slot)
	{
		UE_LOGF(LogArcTactics, Error, "ArcTactics: AddConfig %ls on entity %ls has no tactics instance.",
			*Config.GetName(), *Entity.DebugGetDescription());
		return false;
	}
	if (Slot->ExecutionDepth > 0)
	{
		Slot->PendingPackageChanges.Add(FArcTacticsPendingPackageChange{&Config, true});
		return true;
	}
	FArcTacticsExecutionScope Scope(*this, Handle);
	ApplyAddConfig(Handle, Config);
	return true;
}

bool UArcTacticsInstanceSubsystem::RemoveConfig(const FMassEntityHandle Entity, const UArcTacticsConfig& Config)
{
	check(IsInGameThread());
	const FArcTacticsInstanceHandle Handle = FindHandle(Entity);
	FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	if (!Slot)
	{
		UE_LOGF(LogArcTactics, Error, "ArcTactics: RemoveConfig %ls on entity %ls has no tactics instance.",
			*Config.GetName(), *Entity.DebugGetDescription());
		return false;
	}
	const int32 PackageIndex = Slot->FindPackageIndex(Config);
	int32 RuntimeCount = PackageIndex != INDEX_NONE ? Slot->Packages[PackageIndex]->RuntimeCount : 0;
	for (int32 ChangeIndex = Slot->PendingApplyIndex; ChangeIndex < Slot->PendingPackageChanges.Num(); ++ChangeIndex)
	{
		const FArcTacticsPendingPackageChange& Change = Slot->PendingPackageChanges[ChangeIndex];
		if (Change.Config == &Config)
		{
			RuntimeCount += Change.bAdd ? 1 : -1;
		}
	}
	if (RuntimeCount <= 0)
	{
		UE_LOGF(LogArcTactics, Error, "ArcTactics: RemoveConfig %ls on entity %ls was not added at runtime.",
			*Config.GetName(), *Entity.DebugGetDescription());
		return false;
	}
	if (Slot->ExecutionDepth > 0)
	{
		Slot->PendingPackageChanges.Add(FArcTacticsPendingPackageChange{&Config, false});
		return true;
	}
	FArcTacticsExecutionScope Scope(*this, Handle);
	ApplyRemoveConfig(Handle, Config);
	return true;
}

void UArcTacticsInstanceSubsystem::ApplyAddConfig(
	const FArcTacticsInstanceHandle Handle,
	const UArcTacticsConfig& Config)
{
	FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	UWorld* World = GetWorld();
	FMassEntityManager* EntityManager = World ? UE::Mass::Utils::GetEntityManager(World) : nullptr;
	if (!Slot || !EntityManager)
	{
		return;
	}
	const int32 PackageIndex = Slot->FindPackageIndex(Config);
	if (PackageIndex != INDEX_NONE)
	{
		++Slot->Packages[PackageIndex]->RuntimeCount;
		return;
	}
	FArcTacticsPackage& Package = BuildPackage(*EntityManager, *Slot, Config);
	Package.RuntimeCount = 1;
	if (!Package.bActive)
	{
		return;
	}
	if (Slot->DecisionStatus == EArcTacticsDecisionStatus::Running)
	{
		FArcTacticsTaskContext TaskContext = ArcTacticsInstanceSubsystemPrivate::MakeTaskContext(
			*this, *EntityManager, *Slot, Handle);
		ArcTacticsDecision::CancelDecision(*Slot, TaskContext);
	}
	RequestDecision(Handle);
}

void UArcTacticsInstanceSubsystem::ApplyRemoveConfig(
	const FArcTacticsInstanceHandle Handle,
	const UArcTacticsConfig& Config)
{
	FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	UWorld* World = GetWorld();
	FMassEntityManager* EntityManager = World ? UE::Mass::Utils::GetEntityManager(World) : nullptr;
	const int32 PackageIndex = Slot ? Slot->FindPackageIndex(Config) : INDEX_NONE;
	if (!Slot || !World || !EntityManager || PackageIndex == INDEX_NONE
		|| Slot->Packages[PackageIndex]->RuntimeCount <= 0)
	{
		return;
	}
	FArcTacticsPackage& Package = *Slot->Packages[PackageIndex];
	--Package.RuntimeCount;
	if (Package.RuntimeCount > 0 || Package.StaticCount > 0)
	{
		return;
	}
	const uint32 PackageSerial = Package.Serial;
	if (Slot->DecisionStatus == EArcTacticsDecisionStatus::Running)
	{
		FArcTacticsTaskContext TaskContext = ArcTacticsInstanceSubsystemPrivate::MakeTaskContext(
			*this, *EntityManager, *Slot, Handle);
		ArcTacticsDecision::CancelDecision(*Slot, TaskContext);
		Slot = Resolve(Handle);
		if (!Slot)
		{
			return;
		}
	}
	if (Slot->ActiveAction.IsSet() && Slot->ActiveAction.PackageSerial == PackageSerial)
	{
		const TOptional<ArcPathFollow::FContext> PathContext =
			ArcTacticsInstanceSubsystemPrivate::MakePathContext(*World, *EntityManager, Slot->Entity);
		FArcTacticsActionView ActionView;
		if (const FArcTacticsActionNode* Action = ArcTacticsInstances::ResolveActiveAction(*Slot, ActionView))
		{
			FArcTacticsActionContext ActionContext = ArcTacticsInstances::MakeActionContext(
				*this, *EntityManager, *Slot, Handle, PathContext.GetPtrOrNull());
			Action->Cancel(ActionContext, ActionView.ActionData, EArcTacticsActionCancelMode::ReleaseHold);
			Slot = Resolve(Handle);
			if (!Slot)
			{
				return;
			}
		}
		if (PathContext.IsSet())
		{
			FArcTacticsMovementRuntimeData EmptyRuntime;
			ArcTacticsMovement::ReleaseHold(PathContext.GetValue(), EmptyRuntime, Slot->MovementHold);
		}
		ArcTacticsInstances::RecordEndedAction(
			*Slot, *Slot->Packages[PackageIndex], *World, *EntityManager,
			EArcTacticsActionEndStatus::Cancelled, EArcTacticsActionEndReason::PackageRemoved, NAME_None);
		Slot->ClearActiveAction();
		Slot->bActionEntered = false;
		Slot->ActionWakeSerial = 0;
		Slot->ActionWakeTime = 0.0;
	}
	Slot->Packages.RemoveAt(PackageIndex);
	if (Slot->ActiveAction.IsSet() && Slot->ActiveAction.PackageIndex > PackageIndex)
	{
		--Slot->ActiveAction.PackageIndex;
	}
	RequestDecision(Handle);
}

void UArcTacticsInstanceSubsystem::CancelSlot(
	FMassEntityManager& EntityManager,
	FArcTacticsInstanceSlot& Slot,
	const FArcTacticsInstanceHandle Handle,
	const ArcPathFollow::FContext* PathContext,
	const EArcTacticsActionCancelMode CancelMode)
{
	FArcTacticsExecutionScope Scope(*this, Handle);
	FArcTacticsTaskContext TaskContext = ArcTacticsInstanceSubsystemPrivate::MakeTaskContext(
		*this, EntityManager, Slot, Handle);
	ArcTacticsDecision::CancelDecision(Slot, TaskContext);
	FArcTacticsInstanceSlot* Current = Resolve(Handle);
	if (Current != &Slot)
	{
		return;
	}
	FArcTacticsActionView ActionView;
	if (const FArcTacticsActionNode* Action = ArcTacticsInstances::ResolveActiveAction(Slot, ActionView))
	{
		FArcTacticsActionContext ActionContext = ArcTacticsInstances::MakeActionContext(
			*this, EntityManager, Slot, Handle, PathContext);
		Action->Cancel(ActionContext, ActionView.ActionData, CancelMode);
		Current = Resolve(Handle);
		if (Current != &Slot)
		{
			return;
		}
	}
	if (CancelMode == EArcTacticsActionCancelMode::ReleaseHold && PathContext)
	{
		FArcTacticsMovementRuntimeData EmptyRuntime;
		ArcTacticsMovement::ReleaseHold(*PathContext, EmptyRuntime, Slot.MovementHold);
	}
	ArcTacticsInstances::RecordLastEndedAction(
		Slot, GetWorld(), EArcTacticsActionEndStatus::Cancelled, EArcTacticsActionEndReason::Teardown, NAME_None);
	TRACE_ARCTACTICS_ACTION_ENDED(GetWorld(), &EntityManager, Slot, EArcTacticsActionEndStatus::Cancelled,
		EArcTacticsActionEndReason::Teardown, false, false);
	Slot.ClearActiveAction();
	Slot.bActionEntered = false;
	Slot.bDecisionRequested = false;
	Slot.ActionWakeSerial = 0;
	Slot.ActionWakeTime = 0.0;
}

void UArcTacticsInstanceSubsystem::FreeSlot(const FArcTacticsInstanceHandle Handle)
{
	FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	if (!Slot)
	{
		return;
	}
	if (UArcGameEventSubsystem* Events = GameEventSubsystem.Get())
	{
		Events->Unsubscribe(Slot->AbilityActivatedHandle);
		Events->Unsubscribe(Slot->AbilityEndedHandle);
		Events->Unsubscribe(Slot->AbilityCooldownExpiredHandle);
	}
	const int32 SlotIndex = Handle.Index;
	const uint32 Generation = Slot->Generation;
	*Slot = FArcTacticsInstanceSlot();
	if (Generation == MAX_uint32)
	{
		Slot->Generation = 0;
		Slot->bRetired = true;
		return;
	}
	Slot->Generation = Generation + 1;
	Freelist.Add(SlotIndex);
}

void UArcTacticsInstanceSubsystem::CancelAndFreeInstance(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const FArcTacticsInstanceHandle Handle,
	const ArcPathFollow::FContext* PathContext)
{
	FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	if (!Slot || Slot->Entity != Entity)
	{
		return;
	}
	CancelSlot(
		EntityManager, *Slot, Handle, PathContext, EArcTacticsActionCancelMode::ReleaseHold);
	FreeSlot(Handle);
}

FArcTacticsInstanceHandle UArcTacticsInstanceSubsystem::ReinitializeInstance(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const FArcTacticsInstanceHandle Handle,
	const TConstArrayView<TObjectPtr<const UArcTacticsConfig>> Configs,
	const FArcTacticsDecisionSettings& Settings,
	const ArcPathFollow::FContext* PathContext)
{
	TArray<TPair<TObjectPtr<const UArcTacticsConfig>, int32>> RuntimePackages;
	uint32 NextActionSerial = 0;
	FArcTacticsInstanceSlot* Slot = Resolve(Handle);
	if (Slot && Slot->Entity == Entity)
	{
		CancelSlot(EntityManager, *Slot, Handle, PathContext, EArcTacticsActionCancelMode::ReleaseHold);
		Slot = Resolve(Handle);
		if (Slot)
		{
			for (const TUniquePtr<FArcTacticsPackage>& Package : Slot->Packages)
			{
				if (Package->Config && Package->RuntimeCount > 0)
				{
					RuntimePackages.Emplace(Package->Config, Package->RuntimeCount);
				}
			}
			NextActionSerial = Slot->NextActionSerial;
			FreeSlot(Handle);
		}
	}
	const FArcTacticsInstanceHandle NewHandle = AllocateInstance(EntityManager, Entity, Configs, Settings);
	FArcTacticsInstanceSlot* NewSlot = Resolve(NewHandle);
	if (!NewSlot)
	{
		return NewHandle;
	}
	NewSlot->NextActionSerial = NextActionSerial;
	for (const TPair<TObjectPtr<const UArcTacticsConfig>, int32>& RuntimePackage : RuntimePackages)
	{
		const int32 PackageIndex = NewSlot->FindPackageIndex(*RuntimePackage.Key);
		if (PackageIndex != INDEX_NONE)
		{
			NewSlot->Packages[PackageIndex]->RuntimeCount += RuntimePackage.Value;
			continue;
		}
		BuildPackage(EntityManager, *NewSlot, *RuntimePackage.Key).RuntimeCount = RuntimePackage.Value;
	}
	return NewHandle;
}

void UArcTacticsInstanceSubsystem::CompleteTaskQuery(
	const FArcTacticsAsyncRequestToken& Token,
	FArcTQSQueryInstance& CompletedQuery)
{
	FArcTacticsExecutionScope Scope(*this, Token.InstanceHandle);
	FArcTacticsInstanceSlot* Slot = Resolve(Token.InstanceHandle);
	UWorld* World = GetWorld();
	FMassEntityManager* EntityManager = World ? UE::Mass::Utils::GetEntityManager(World) : nullptr;
	if (Token.InstanceSubsystem.Get() != this || !Slot || !EntityManager
		|| Slot->Entity != Token.Entity || !EntityManager->IsEntityActive(Token.Entity)
		|| Slot->DecisionGeneration != Token.DecisionGeneration
		|| Slot->DecisionStatus != EArcTacticsDecisionStatus::Running
		|| !Slot->Packages.IsValidIndex(Slot->PackageCursor))
	{
		return;
	}
	FArcTacticsPackage& Package = *Slot->Packages[Slot->PackageCursor];
	const TSharedPtr<const FArcTacticsCompiledData> Definitions = Package.Definitions;
	if (!Package.bActive || Package.bExcluded || !Definitions
		|| !Package.Instance.bGlobalTaskRunning
		|| !Definitions->GlobalTasks.IsValidIndex(Package.Instance.GlobalTaskIndex))
	{
		return;
	}
	const FArcTacticsNodeHandle TaskHandle = Definitions->GlobalTasks[Package.Instance.GlobalTaskIndex];
	if (!Definitions->Nodes.IsValidIndex(TaskHandle.Get()))
	{
		return;
	}
	const FArcTacticsCompiledNode& CompiledNode = Definitions->Nodes[TaskHandle.Get()];
	const FArcTacticsGlobalTask* Task = CompiledNode.Node.GetPtr<FArcTacticsGlobalTask>();
	const int32 DataIndex = CompiledNode.InstanceData.GetIndex();
	if (!Task || CompiledNode.InstanceData.GetKind() != EArcTacticsBindingEndpointKind::Node
		|| !Package.Instance.NodeInstanceData.IsValidIndex(DataIndex))
	{
		return;
	}
	const uint32 PackageSerial = Package.Serial;
	const int32 PackageCursor = Slot->PackageCursor;
	FArcTacticsTaskContext Context = ArcTacticsInstanceSubsystemPrivate::MakeTaskContext(
		*this, *EntityManager, *Slot, Token.InstanceHandle);
	Context.DecisionGeneration = Token.DecisionGeneration;
	Task->OnAsyncQueryCompleted(
		Context, Package.Instance.NodeInstanceData[DataIndex], Token, CompletedQuery);
	TRACE_ARCTACTICS_TQS_QUERY(World, EntityManager, Token.Entity, Token.DecisionGeneration,
		EArcTacticsQuerySource::GlobalTask, CompiledNode.Name, 0, Context.QueryReport, CompletedQuery);
	Slot = Resolve(Token.InstanceHandle);
	if (Slot && Slot->Entity == Token.Entity
		&& Slot->DecisionGeneration == Token.DecisionGeneration
		&& Slot->DecisionStatus == EArcTacticsDecisionStatus::Running
		&& Slot->PackageCursor == PackageCursor
		&& Slot->Packages.IsValidIndex(PackageCursor)
		&& Slot->Packages[PackageCursor]->Serial == PackageSerial
		&& Slot->Packages[PackageCursor]->Instance.bGlobalTaskRunning)
	{
		SignalEntity(UE::ArcTactics::Signals::TaskWakeup, Token.Entity);
	}
}

void UArcTacticsInstanceSubsystem::CompleteActionQuery(
	const FArcTacticsAsyncRequestToken& Token,
	FArcTQSQueryInstance& CompletedQuery)
{
	FArcTacticsExecutionScope Scope(*this, Token.InstanceHandle);
	FArcTacticsInstanceSlot* Slot = Resolve(Token.InstanceHandle);
	UWorld* World = GetWorld();
	FMassEntityManager* EntityManager = World ? UE::Mass::Utils::GetEntityManager(World) : nullptr;
	if (Token.InstanceSubsystem.Get() != this || !Slot || !EntityManager
		|| Slot->Entity != Token.Entity || !EntityManager->IsEntityActive(Token.Entity)
		|| !Slot->ActiveAction.IsSet()
		|| Slot->ActiveAction.Serial != Token.ActionSerial)
	{
		return;
	}
	FArcTacticsActionView ActionView;
	const FArcTacticsActionNode* Action = ArcTacticsInstances::ResolveActiveAction(*Slot, ActionView);
	if (!Action)
	{
		return;
	}
	const TSharedPtr<const FArcTacticsCompiledData> Definitions = ArcTacticsInstances::ResolveActivePackage(*Slot)->Definitions;
	FArcTacticsActionContext Context = ArcTacticsInstances::MakeActionContext(
		*this, *EntityManager, *Slot, Token.InstanceHandle, nullptr);
	Context.RequestToken = Token;
	Action->OnAsyncQueryCompleted(Context, ActionView.ActionData, Token, CompletedQuery);
	TRACE_ARCTACTICS_TQS_QUERY(World, EntityManager, Token.Entity, Token.DecisionGeneration,
		EArcTacticsQuerySource::Step, Context.QueryReport.SourceName, Token.ActionSerial, Context.QueryReport, CompletedQuery);
	Slot = Resolve(Token.InstanceHandle);
	if (Slot && Slot->Entity == Token.Entity && Slot->ActiveAction.IsSet()
		&& Slot->ActiveAction.Serial == Token.ActionSerial)
	{
		Slot->ActionWakeSerial = Token.ActionSerial;
		Slot->ActionWakeTime = World->GetTimeSeconds();
		SignalEntity(UE::ArcTactics::Signals::ActionWakeup, Token.Entity);
	}
}

void UArcTacticsInstanceSubsystem::HandleAbilityActivated(const FArcGameEvent& Event)
{
	SignalEntity(UE::ArcTactics::Signals::AbilityWakeup, Event.Target);
}

void UArcTacticsInstanceSubsystem::HandleAbilityEnded(const FArcGameEvent& Event)
{
	SignalEntity(UE::ArcTactics::Signals::AbilityWakeup, Event.Target);
}

void UArcTacticsInstanceSubsystem::HandleAbilityCooldownExpired(const FArcGameEvent& Event)
{
	RequestDecision(Event.Target);
}

void UArcTacticsInstanceSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UArcTacticsInstanceSubsystem* This = CastChecked<UArcTacticsInstanceSubsystem>(InThis);
	for (const TUniquePtr<FArcTacticsInstanceSlot>& SlotPtr : This->Slots)
	{
		if (SlotPtr && SlotPtr->bAllocated)
		{
			SlotPtr->AddReferencedObjects(Collector, This);
		}
	}
	Super::AddReferencedObjects(InThis, Collector);
}
