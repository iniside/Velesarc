// Copyright Lukasz Baran. All Rights Reserved.

#include "Debugger/ArcTacticsDebug.h"

#include "Actions/ArcTacticsAction.h"
#include "Actions/ArcTacticsSequenceAction.h"
#include "Decision/ArcTacticsConfig.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"
#include "Mass/EntityView.h"
#include "Memory/ArcMassMemory.h"
#include "Misc/OutputDevice.h"
#include "UtilityAI/ArcUtilityScoring.h"

namespace ArcTacticsDebugPrivate
{
	TWeakObjectPtr<UWorld> CapturedWorld;
	FMassEntityHandle CapturedEntity;
#if WITH_ARCTACTICS_TRACE
	FArcTacticsDebugLastDecision CapturedDecision;

	void OpenDecision(const UWorld& World, const FArcTacticsInstanceSlot& Slot)
	{
		CapturedDecision = FArcTacticsDebugLastDecision();
		CapturedDecision.bValid = true;
		CapturedDecision.DecisionGeneration = Slot.DecisionGeneration;
		CapturedDecision.Time = World.GetTimeSeconds();
	}
#endif

	const TCHAR* OutcomeText(const EArcTacticsCandidateOutcome Outcome)
	{
		switch (Outcome)
		{
		case EArcTacticsCandidateOutcome::Accepted:
			return TEXT("Accepted");
		case EArcTacticsCandidateOutcome::BelowMinScore:
			return TEXT("BelowMinScore");
		case EArcTacticsCandidateOutcome::Vetoed:
			return TEXT("Vetoed");
		case EArcTacticsCandidateOutcome::BindingFailed:
			return TEXT("BindingFailed");
		}
		return TEXT("Unknown");
	}

	const TCHAR* SelectionModeText(const EArcTacticsSelectionMode Mode)
	{
		switch (Mode)
		{
		case EArcTacticsSelectionMode::HighestScore:
			return TEXT("HighestScore");
		case EArcTacticsSelectionMode::RandomFromTopPercent:
			return TEXT("RandomFromTopPercent");
		case EArcTacticsSelectionMode::WeightedRandom:
			return TEXT("WeightedRandom");
		}
		return TEXT("Unknown");
	}

	const TCHAR* DecisionStatusText(const EArcTacticsDecisionStatus Status)
	{
		switch (Status)
		{
		case EArcTacticsDecisionStatus::Idle:
			return TEXT("Idle");
		case EArcTacticsDecisionStatus::Running:
			return TEXT("Running");
		case EArcTacticsDecisionStatus::Succeeded:
			return TEXT("Succeeded");
		case EArcTacticsDecisionStatus::Failed:
			return TEXT("Failed");
		case EArcTacticsDecisionStatus::Cancelled:
			return TEXT("Cancelled");
		}
		return TEXT("Unknown");
	}

	const TCHAR* LaneText(const EArcTacticsActionTickLane Lane)
	{
		switch (Lane)
		{
		case EArcTacticsActionTickLane::None:
			return TEXT("None");
		case EArcTacticsActionTickLane::Utility:
			return TEXT("Utility");
		case EArcTacticsActionTickLane::Movement:
			return TEXT("Movement");
		}
		return TEXT("Unknown");
	}

	const TCHAR* CooldownStartText(const EArcTacticsCooldownStart Start)
	{
		switch (Start)
		{
		case EArcTacticsCooldownStart::OnEnd:
			return TEXT("OnEnd");
		case EArcTacticsCooldownStart::OnEnter:
			return TEXT("OnEnter");
		}
		return TEXT("Unknown");
	}

	const TCHAR* BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString IdentityText(const FArcTacticsCandidateIdentity& Identity)
	{
		switch (Identity.Type)
		{
		case EArcTacticsValueIdentity::Entity:
			return Identity.Entity.DebugGetDescription();
		case EArcTacticsValueIdentity::Vector:
			return Identity.Vector.ToCompactString();
		case EArcTacticsValueIdentity::None:
			return TEXT("None");
		}
		return TEXT("Unknown");
	}

	FString SourceText(const bool bFromSelector, const int32 SourceIndex)
	{
		return FString::Printf(TEXT("%s[%d]"), bFromSelector ? TEXT("selector") : TEXT("action"), SourceIndex);
	}

	FString NodeTypeName(const FArcTacticsCompiledData& CompiledData, const FArcTacticsNodeHandle Handle)
	{
		const int32 NodeIndex = Handle.Get();
		const FArcTacticsNode* Node = CompiledData.Nodes.IsValidIndex(NodeIndex)
			? CompiledData.Nodes[NodeIndex].Node.GetPtr<FArcTacticsNode>()
			: nullptr;
		return Node ? Node->GetDisplayName().ToString() : FString();
	}

	FName NodeName(const FArcTacticsCompiledData& CompiledData, const FArcTacticsNodeHandle Handle)
	{
		const int32 NodeIndex = Handle.Get();
		return CompiledData.Nodes.IsValidIndex(NodeIndex) ? CompiledData.Nodes[NodeIndex].Name : NAME_None;
	}

	FString PackageName(const FArcTacticsInstanceSlot& Slot, const int32 PackageIndex)
	{
		return Slot.Packages.IsValidIndex(PackageIndex) ? GetNameSafe(Slot.Packages[PackageIndex]->Config) : FString();
	}

	void AddBranch(
		const FArcTacticsInstanceSlot& Slot,
		const FArcTacticsPackage& Package,
		const FArcTacticsCompiledData& Definitions,
		const double Now,
		const int32 ActiveStepIndex,
		const bool bActiveStepsEntered,
		const bool bSelector,
		const int32 SourceIndex,
		const FName Name,
		const FArcTacticsNodeHandle ActionHandle,
		const FArcTacticsNodeHandle SelectorHandle,
		const TConstArrayView<FArcTacticsNodeHandle> Considerations,
		const TConstArrayView<FArcTacticsNodeHandle> Steps,
		const TConstArrayView<FArcTacticsNodeHandle> ActiveSteps,
		const FArcTacticsBranchParameters& DefaultParameters,
		TArray<FArcTacticsDebugBranch>& OutBranches)
	{
		const FArcTacticsInstanceData& Instance = Package.Instance;
		const int32 BranchIndex = bSelector ? Definitions.Actions.Num() + SourceIndex : SourceIndex;
		const FArcTacticsBranchParameters& Parameters = Instance.BranchParameters.IsValidIndex(BranchIndex)
			? Instance.BranchParameters[BranchIndex]
			: DefaultParameters;
		FArcTacticsDebugBranch& Branch = OutBranches.AddDefaulted_GetRef();
		Branch.Name = Name;
		Branch.bSelector = bSelector;
		Branch.SourceIndex = SourceIndex;
		Branch.TypeName = NodeTypeName(Definitions, ActionHandle);
		Branch.SelectorTypeName = bSelector ? NodeTypeName(Definitions, SelectorHandle) : FString();
		Branch.Priority = Parameters.Priority;
		Branch.Weight = Parameters.Weight;
		Branch.Cooldown = Parameters.Cooldown;
		Branch.CooldownStart = Parameters.CooldownStart;
		const double* CooldownEndTime = Slot.CooldownEndTimes.Find(
			FArcTacticsCooldownKey(Package.Config.Get(), BranchIndex));
		Branch.CooldownRemaining = CooldownEndTime ? FMath::Max(0.0, *CooldownEndTime - Now) : 0.0;
		Branch.bActive = Slot.ActiveAction.IsSet()
			&& Slot.ActiveAction.PackageSerial == Package.Serial
			&& FArcTacticsInstanceData::GetBranchIndex(Definitions, Slot.ActiveAction) == BranchIndex;
		for (const FArcTacticsNodeHandle Consideration : Considerations)
		{
			Branch.Considerations.Add(NodeName(Definitions, Consideration));
		}
		for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
		{
			FArcTacticsDebugStep& Step = Branch.Steps.AddDefaulted_GetRef();
			Step.Name = NodeName(Definitions, Steps[StepIndex]);
			Step.TypeName = NodeTypeName(Definitions, Steps[StepIndex]);
			Step.bCurrent = Branch.bActive && StepIndex == ActiveStepIndex;
		}
		for (const FArcTacticsNodeHandle ActiveStepHandle : ActiveSteps)
		{
			FArcTacticsDebugActiveStep& ActiveStep = Branch.ActiveSteps.AddDefaulted_GetRef();
			ActiveStep.Name = NodeName(Definitions, ActiveStepHandle);
			ActiveStep.TypeName = NodeTypeName(Definitions, ActiveStepHandle);
			ActiveStep.bEntered = Branch.bActive && bActiveStepsEntered;
		}
	}

	void DumpEntity(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		if (!World || Args.Num() != 2)
		{
			Output.Log(TEXT("Usage: arc.Tactics.Dump <EntityIndex> <EntitySerial>"));
			return;
		}
		int32 EntityIndex = 0;
		int32 EntitySerial = 0;
		if (!LexTryParseString(EntityIndex, *Args[0]) || !LexTryParseString(EntitySerial, *Args[1]))
		{
			Output.Log(TEXT("arc.Tactics.Dump requires integer entity index and serial arguments."));
			return;
		}
		const FMassEntityHandle Entity(EntityIndex, EntitySerial);
		ArcTacticsDebug::SetCaptureTarget(*World, Entity);
		FArcTacticsDebugSnapshot Snapshot;
		if (!ArcTacticsDebug::BuildEntitySnapshot(*World, Entity, Snapshot))
		{
			Output.Logf(TEXT("ArcTactics entity [%d:%d] is unavailable."), EntityIndex, EntitySerial);
			return;
		}
		TArray<FString> Lines;
		ArcTacticsDebug::FormatSnapshot(Snapshot, Lines);
		for (const FString& Line : Lines)
		{
			Output.Log(*Line);
		}
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice DumpCommand(
		TEXT("arc.Tactics.Dump"),
		TEXT("Dump one ArcTactics pool instance and arm its shared diagnostics filter."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpEntity));
}

bool ArcTacticsDebug::IsCapturing(const UWorld& World, const FMassEntityHandle Entity)
{
	return ArcTacticsDebugPrivate::CapturedWorld.Get() == &World
		&& ArcTacticsDebugPrivate::CapturedEntity == Entity;
}

void ArcTacticsDebug::SetCaptureTarget(UWorld& World, const FMassEntityHandle Entity)
{
	if (IsCapturing(World, Entity))
	{
		return;
	}
	ArcTacticsDebugPrivate::CapturedWorld = &World;
	ArcTacticsDebugPrivate::CapturedEntity = Entity;
#if WITH_ARCTACTICS_TRACE
	ArcTacticsDebugPrivate::CapturedDecision = FArcTacticsDebugLastDecision();
#endif
}

FMassEntityHandle ArcTacticsDebug::GetCaptureTarget(const UWorld& World)
{
	return ArcTacticsDebugPrivate::CapturedWorld.Get() == &World
		? ArcTacticsDebugPrivate::CapturedEntity
		: FMassEntityHandle();
}

void ArcTacticsDebug::ClearCaptureTarget()
{
	ArcTacticsDebugPrivate::CapturedWorld.Reset();
	ArcTacticsDebugPrivate::CapturedEntity.Reset();
#if WITH_ARCTACTICS_TRACE
	ArcTacticsDebugPrivate::CapturedDecision = FArcTacticsDebugLastDecision();
#endif
}

#if WITH_ARCTACTICS_TRACE
void ArcTacticsDebug::CaptureCandidate(
	const UWorld& World,
	const FArcTacticsInstanceSlot& Slot,
	const FArcTacticsCompiledData& CompiledData,
	const FArcTacticsCandidate& Candidate,
	const TConstArrayView<FArcTacticsNodeHandle> Considerations,
	const TConstArrayView<FArcUtilityConsiderationSnapshot> Snapshots,
	const int32 EarlyOutIndex,
	const int32 VetoIndex,
	const TOptional<int32> Priority)
{
	if (!IsCapturing(World, Slot.Context.Self))
	{
		return;
	}
	FArcTacticsDebugLastDecision& Decision = ArcTacticsDebugPrivate::CapturedDecision;
	if (Candidate.Order == 0 || !Decision.bValid || Decision.DecisionGeneration != Slot.DecisionGeneration)
	{
		ArcTacticsDebugPrivate::OpenDecision(World, Slot);
	}
	FArcTacticsDebugCandidate& Captured = Decision.Candidates.AddDefaulted_GetRef();
	Captured.Order = Candidate.Order;
	Captured.Package = ArcTacticsDebugPrivate::PackageName(Slot, Candidate.Option.PackageIndex);
	Captured.Name = Candidate.Option.Name;
	Captured.bFromSelector = Candidate.Option.bFromSelector;
	Captured.SourceIndex = Candidate.Option.SourceIndex;
	Captured.ItemIndex = Candidate.Option.ItemIndex;
	Captured.Target = Candidate.Option.Identity;
	Captured.Score = Candidate.Option.Score;
	Captured.Outcome = Candidate.Outcome;
	Captured.Reason = Candidate.RejectionReason;
	Captured.Priority = Priority.Get(0);
	Captured.bHasPriority = Priority.IsSet();
	Captured.EarlyOutIndex = EarlyOutIndex;
	Captured.VetoIndex = VetoIndex;
	Captured.Considerations.Reserve(Considerations.Num());
	for (const FArcTacticsNodeHandle Consideration : Considerations)
	{
		Captured.Considerations.AddDefaulted_GetRef().Name =
			ArcTacticsDebugPrivate::NodeName(CompiledData, Consideration);
	}
	for (const FArcUtilityConsiderationSnapshot& Snapshot : Snapshots)
	{
		if (Captured.Considerations.IsValidIndex(Snapshot.ConsiderationIndex))
		{
			FArcTacticsDebugConsideration& Consideration = Captured.Considerations[Snapshot.ConsiderationIndex];
			Consideration.Raw = Snapshot.RawScore;
			Consideration.Curved = Snapshot.CurvedScore;
			Consideration.bScored = true;
		}
	}
}

void ArcTacticsDebug::CaptureDecision(
	const UWorld& World,
	const FArcTacticsInstanceSlot& Slot,
	const int32 SelectedOrder)
{
	if (!IsCapturing(World, Slot.Context.Self))
	{
		return;
	}
	FArcTacticsDebugLastDecision& Decision = ArcTacticsDebugPrivate::CapturedDecision;
	if (Slot.Candidates.IsEmpty() || !Decision.bValid || Decision.DecisionGeneration != Slot.DecisionGeneration)
	{
		ArcTacticsDebugPrivate::OpenDecision(World, Slot);
	}
	Decision.Status = Slot.DecisionStatus;
	Decision.Diagnostic = Slot.Diagnostic;
	Decision.SelectedOrder = SelectedOrder;
}
#endif

bool ArcTacticsDebug::BuildEntitySnapshot(
	UWorld& World,
	const FMassEntityHandle Entity,
	FArcTacticsDebugSnapshot& OutSnapshot)
{
	OutSnapshot = FArcTacticsDebugSnapshot();
	FMassEntityManager* EntityManager = UE::Mass::Utils::GetEntityManager(&World);
	UArcTacticsInstanceSubsystem* Instances = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
	if (!EntityManager || !Instances || !EntityManager->IsEntityActive(Entity))
	{
		return false;
	}
	const FMassEntityView EntityView(*EntityManager, Entity);
	const FArcTacticsInstanceFragment* InstanceFragment =
		EntityView.GetFragmentDataPtr<FArcTacticsInstanceFragment>();
	if (!InstanceFragment)
	{
		return false;
	}
	const double Now = World.GetTimeSeconds();
	OutSnapshot.Entity = Entity;
	OutSnapshot.Now = Now;
	OutSnapshot.bEnabled = InstanceFragment->bEnabled;
	OutSnapshot.Pool = InstanceFragment->Handle;
	FArcTacticsInstanceSlot* Slot = Instances->Resolve(InstanceFragment->Handle);
	OutSnapshot.bPoolValid = Slot != nullptr;
	if (!Slot)
	{
		return true;
	}
	OutSnapshot.DecisionStatus = Slot->DecisionStatus;
	OutSnapshot.DecisionGeneration = Slot->DecisionGeneration;
	OutSnapshot.bDecisionRequested = Slot->bDecisionRequested;
	OutSnapshot.NextDecisionTime = Slot->NextDecisionTime;
	OutSnapshot.DecisionInterval = Slot->Settings.DecisionInterval;
	OutSnapshot.SelectionMode = Slot->Settings.Selection.Mode;
	OutSnapshot.TopPercent = Slot->Settings.Selection.TopPercent;
	OutSnapshot.HistorySize = Slot->Settings.HistorySize;
	OutSnapshot.Diagnostic = Slot->Diagnostic;
	OutSnapshot.PackageCursor = Slot->PackageCursor;
	for (const TPair<FArcTacticsCooldownKey, double>& Cooldown : Slot->CooldownEndTimes)
	{
		FArcTacticsDebugCooldown& Entry = OutSnapshot.Cooldowns.AddDefaulted_GetRef();
		Entry.ConfigName = GetNameSafe(Cooldown.Key.Key.ResolveObjectPtr());
		Entry.BranchIndex = Cooldown.Key.Value;
		Entry.Remaining = FMath::Max(0.0, Cooldown.Value - Now);
	}

	FArcTacticsDebugActiveAction& Active = OutSnapshot.ActiveAction;
	bool bActiveStepsEntered = false;
	if (Slot->ActiveAction.IsSet())
	{
		FArcTacticsActionView ActionView;
		const FArcTacticsActionNode* Action = ArcTacticsInstances::ResolveActiveAction(*Slot, ActionView);
		const FArcTacticsOption& Option = Slot->ActiveAction;
		Active.bSet = true;
		Active.Package = ArcTacticsDebugPrivate::PackageName(*Slot, Option.PackageIndex);
		Active.Name = Option.Name;
		Active.TypeName = Action ? Action->GetDisplayName().ToString() : FString();
		Active.Serial = Option.Serial;
		Active.bFromSelector = Option.bFromSelector;
		Active.SourceIndex = Option.SourceIndex;
		Active.Target = Option.Identity;
		if (Option.Identity.Type == EArcTacticsValueIdentity::Entity)
		{
			const FTransformFragment* TargetTransform = EntityManager->IsEntityActive(Option.Identity.Entity)
				? EntityManager->GetFragmentDataPtr<FTransformFragment>(Option.Identity.Entity)
				: nullptr;
			if (TargetTransform)
			{
				Active.bHasTargetLocation = true;
				Active.TargetLocation = TargetTransform->GetTransform().GetLocation();
			}
		}
		else if (Option.Identity.Type == EArcTacticsValueIdentity::Vector)
		{
			Active.bHasTargetLocation = true;
			Active.TargetLocation = Option.Identity.Vector;
		}
		Active.Priority = Option.Parameters.Priority;
		Active.Score = Option.Score;
		Active.bEntered = Slot->bActionEntered;
		if (Action)
		{
			Active.Lane = Action->GetTickLane(ActionView);
			Active.bBlocksDecision = Action->BlocksDecision(ActionView);
			Active.WakeTime = Action->GetNextWakeTime(ActionView);
			Active.StepIndex = Action->GetCurrentStepIndex(ActionView);
		}
		Active.StepCount = ActionView.Steps.Num();
		Active.StepName = ActionView.Steps.GetName(Active.StepIndex);
		const FArcTacticsStepNode* Step = ActionView.Steps.GetNode(Active.StepIndex);
		Active.StepTypeName = Step ? Step->GetDisplayName().ToString() : FString();
		const FArcTacticsSequenceActionInstanceData* SequenceData =
			ActionView.ActionData.GetPtr<FArcTacticsSequenceActionInstanceData>();
		bActiveStepsEntered = SequenceData && SequenceData->bActiveStepsEntered;
		Active.bHoldOwned = Slot->MovementHold.bOwned;
		Active.HoldActionID = Slot->MovementHold.ActionID;
	}
	const FArcTacticsEndedAction& Ended = Slot->LastEndedAction;
	FArcTacticsDebugEndedAction& DebugEnded = OutSnapshot.LastEndedAction;
	DebugEnded.bSet = Ended.bSet;
	DebugEnded.Package = Ended.ConfigName.ToString();
	DebugEnded.Name = Ended.Name;
	DebugEnded.Status = Ended.Status;
	DebugEnded.Reason = Ended.Reason;
	DebugEnded.FailureReason = Ended.FailureReason;
	DebugEnded.Time = Ended.Time;

	for (const TUniquePtr<FArcTacticsPackage>& PackagePtr : Slot->Packages)
	{
		const FArcTacticsPackage& Package = *PackagePtr;
		const FArcTacticsInstanceData& Instance = Package.Instance;
		FArcTacticsDebugPackage& DebugPackage = OutSnapshot.Packages.AddDefaulted_GetRef();
		DebugPackage.ConfigName = GetNameSafe(Package.Config);
		DebugPackage.SeenRevision = Package.SeenRevision;
		DebugPackage.bActive = Package.bActive;
		DebugPackage.bExcluded = Package.bExcluded;
		DebugPackage.StaticCount = Package.StaticCount;
		DebugPackage.RuntimeCount = Package.RuntimeCount;
		DebugPackage.Diagnostic = Instance.Diagnostic;
		DebugPackage.MinScore = Instance.DecisionSettings.MinScore;
		DebugPackage.GlobalTaskIndex = Instance.GlobalTaskIndex;
		DebugPackage.bGlobalTaskRunning = Instance.bGlobalTaskRunning;
		if (!Package.Definitions)
		{
			continue;
		}
		const FArcTacticsCompiledData& Definitions = *Package.Definitions;
		DebugPackage.Revision = Definitions.Revision;
		if (Definitions.GlobalTasks.IsValidIndex(Instance.GlobalTaskIndex))
		{
			DebugPackage.GlobalTaskName = ArcTacticsDebugPrivate::NodeName(
				Definitions, Definitions.GlobalTasks[Instance.GlobalTaskIndex]);
		}
		for (const FArcTacticsNodeHandle Task : Definitions.GlobalTasks)
		{
			FArcTacticsDebugNode& Node = DebugPackage.GlobalTasks.AddDefaulted_GetRef();
			Node.Name = ArcTacticsDebugPrivate::NodeName(Definitions, Task);
			Node.TypeName = ArcTacticsDebugPrivate::NodeTypeName(Definitions, Task);
		}
		for (int32 ActionIndex = 0; ActionIndex < Definitions.Actions.Num(); ++ActionIndex)
		{
			const FArcTacticsCompiledAction& Action = Definitions.Actions[ActionIndex];
			ArcTacticsDebugPrivate::AddBranch(
				*Slot, Package, Definitions, Now, Active.StepIndex, bActiveStepsEntered, false, ActionIndex, Action.Name,
				Action.Action, FArcTacticsNodeHandle(), Action.Considerations, Action.Steps, Action.ActiveSteps,
				Action.DefaultParameters, DebugPackage.Branches);
		}
		for (int32 SelectorIndex = 0; SelectorIndex < Definitions.Selectors.Num(); ++SelectorIndex)
		{
			const FArcTacticsCompiledSelector& Selector = Definitions.Selectors[SelectorIndex];
			ArcTacticsDebugPrivate::AddBranch(
				*Slot, Package, Definitions, Now, Active.StepIndex, bActiveStepsEntered, true, SelectorIndex, Selector.Name,
				Selector.Action, Selector.Selector, Selector.Considerations, Selector.Steps, Selector.ActiveSteps,
				Selector.DefaultParameters, DebugPackage.Branches);
		}
	}

	const TArray<FArcTacticsActionHistoryEntry>& HistoryEntries = Slot->Context.History.Entries;
	for (int32 EntryIndex = HistoryEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		FArcTacticsDebugHistoryEntry& Entry = OutSnapshot.History.AddDefaulted_GetRef();
		Entry.Tag = HistoryEntries[EntryIndex].Tag;
		Entry.Age = Now - HistoryEntries[EntryIndex].Time;
	}

	if (const FArcMassMemoryFragment* Memory = ArcMassMemory::FindFragment(*EntityManager, Entity))
	{
		OutSnapshot.bHasMemory = true;
		for (const TPair<FGameplayTag, FInstancedStruct>& Value : Memory->Values)
		{
			FArcTacticsDebugMemoryEntry& Entry = OutSnapshot.Memory.AddDefaulted_GetRef();
			Entry.Key = Value.Key;
			const UScriptStruct* ValueType = Value.Value.GetScriptStruct();
			Entry.TypeName = GetNameSafe(ValueType);
			if (ValueType)
			{
				ValueType->ExportText(Entry.ValueText, Value.Value.GetMemory(), nullptr, nullptr, PPF_None, nullptr);
			}
		}
		OutSnapshot.Memory.Sort([](const FArcTacticsDebugMemoryEntry& A, const FArcTacticsDebugMemoryEntry& B)
		{
			return A.Key.ToString() < B.Key.ToString();
		});
	}

#if WITH_ARCTACTICS_TRACE
	if (IsCapturing(World, Entity) && ArcTacticsDebugPrivate::CapturedDecision.bValid)
	{
		OutSnapshot.LastDecision = ArcTacticsDebugPrivate::CapturedDecision;
		for (FArcTacticsDebugCandidate& Candidate : OutSnapshot.LastDecision.Candidates)
		{
			Candidate.bSelected = Candidate.Order != INDEX_NONE
				&& Candidate.Order == OutSnapshot.LastDecision.SelectedOrder;
		}
	}
#endif
	return true;
}

const TCHAR* ArcTacticsDebug::EndStatusText(const EArcTacticsActionEndStatus Status)
{
	switch (Status)
	{
	case EArcTacticsActionEndStatus::Succeeded:
		return TEXT("Succeeded");
	case EArcTacticsActionEndStatus::Failed:
		return TEXT("Failed");
	case EArcTacticsActionEndStatus::Cancelled:
		return TEXT("Cancelled");
	}
	return TEXT("Unknown");
}

const TCHAR* ArcTacticsDebug::EndReasonText(const EArcTacticsActionEndReason Reason)
{
	switch (Reason)
	{
	case EArcTacticsActionEndReason::Completed:
		return TEXT("Completed");
	case EArcTacticsActionEndReason::EnterEnded:
		return TEXT("EnterEnded");
	case EArcTacticsActionEndReason::Replaced:
		return TEXT("Replaced");
	case EArcTacticsActionEndReason::Unresolved:
		return TEXT("Unresolved");
	case EArcTacticsActionEndReason::InputsFailed:
		return TEXT("InputsFailed");
	case EArcTacticsActionEndReason::Teardown:
		return TEXT("Teardown");
	case EArcTacticsActionEndReason::PackageRemoved:
		return TEXT("PackageRemoved");
	}
	return TEXT("Unknown");
}

void ArcTacticsDebug::FormatSnapshot(const FArcTacticsDebugSnapshot& Snapshot, TArray<FString>& OutLines)
{
	using namespace ArcTacticsDebugPrivate;
	OutLines.Reset();
	OutLines.Add(FString::Printf(
		TEXT("ArcTactics [%d:%d] enabled=%s pool=[%d:%u] valid=%s"),
		Snapshot.Entity.Index, Snapshot.Entity.SerialNumber, BoolText(Snapshot.bEnabled),
		Snapshot.Pool.Index, Snapshot.Pool.Generation, BoolText(Snapshot.bPoolValid)));
	if (!Snapshot.bPoolValid)
	{
		return;
	}
	OutLines.Add(FString::Printf(
		TEXT("  decision status=%s generation=%u requested=%s next=%.3f interval=%.3f policy=%s top=%.3f history=%d cursor=%d diagnostic=%s"),
		DecisionStatusText(Snapshot.DecisionStatus), Snapshot.DecisionGeneration,
		BoolText(Snapshot.bDecisionRequested), Snapshot.NextDecisionTime, Snapshot.DecisionInterval,
		SelectionModeText(Snapshot.SelectionMode), Snapshot.TopPercent, Snapshot.HistorySize,
		Snapshot.PackageCursor, *Snapshot.Diagnostic.ToString()));
	for (const FArcTacticsDebugCooldown& Cooldown : Snapshot.Cooldowns)
	{
		OutLines.Add(FString::Printf(
			TEXT("  cooldown config=%s branch=%d remaining=%.1fs"),
			*Cooldown.ConfigName, Cooldown.BranchIndex, Cooldown.Remaining));
	}

	for (int32 PackageIndex = 0; PackageIndex < Snapshot.Packages.Num(); ++PackageIndex)
	{
		const FArcTacticsDebugPackage& Package = Snapshot.Packages[PackageIndex];
		OutLines.Add(FString::Printf(
			TEXT("  package[%d] config=%s seen=%u revision=%u active=%s excluded=%s static=%d runtime=%d min=%.3f task=%d:%s running=%s diagnostic=%s"),
			PackageIndex, *Package.ConfigName, Package.SeenRevision, Package.Revision,
			BoolText(Package.bActive), BoolText(Package.bExcluded), Package.StaticCount, Package.RuntimeCount,
			Package.MinScore, Package.GlobalTaskIndex, *Package.GlobalTaskName.ToString(),
			BoolText(Package.bGlobalTaskRunning), *Package.Diagnostic.ToString()));
		OutLines.Add(TEXT("    global tasks:"));
		for (int32 TaskIndex = 0; TaskIndex < Package.GlobalTasks.Num(); ++TaskIndex)
		{
			const FArcTacticsDebugNode& Task = Package.GlobalTasks[TaskIndex];
			OutLines.Add(FString::Printf(
				TEXT("      task[%d] %s (%s)"), TaskIndex, *Task.Name.ToString(), *Task.TypeName));
		}
		OutLines.Add(TEXT("    branches:"));
		for (const FArcTacticsDebugBranch& Branch : Package.Branches)
		{
			OutLines.Add(FString::Printf(
				TEXT("      %s %s (%s%s%s) priority=%d weight=%.3f cooldown=%.2f start=%s remaining=%.1fs active=%s"),
				*SourceText(Branch.bSelector, Branch.SourceIndex), *Branch.Name.ToString(),
				*Branch.SelectorTypeName, Branch.bSelector ? TEXT(" > ") : TEXT(""), *Branch.TypeName,
				Branch.Priority, Branch.Weight, Branch.Cooldown, CooldownStartText(Branch.CooldownStart),
				Branch.CooldownRemaining, BoolText(Branch.bActive)));
			for (const FName Consideration : Branch.Considerations)
			{
				OutLines.Add(FString::Printf(TEXT("        consideration %s"), *Consideration.ToString()));
			}
			for (int32 StepIndex = 0; StepIndex < Branch.Steps.Num(); ++StepIndex)
			{
				const FArcTacticsDebugStep& Step = Branch.Steps[StepIndex];
				OutLines.Add(FString::Printf(
					TEXT("        %sstep[%d] %s (%s)"), Step.bCurrent ? TEXT("> ") : TEXT(""),
					StepIndex, *Step.Name.ToString(), *Step.TypeName));
			}
			for (int32 ActiveStepIndex = 0; ActiveStepIndex < Branch.ActiveSteps.Num(); ++ActiveStepIndex)
			{
				const FArcTacticsDebugActiveStep& ActiveStep = Branch.ActiveSteps[ActiveStepIndex];
				OutLines.Add(FString::Printf(
					TEXT("        while-active[%d] %s (%s) entered=%s"),
					ActiveStepIndex, *ActiveStep.Name.ToString(), *ActiveStep.TypeName, BoolText(ActiveStep.bEntered)));
			}
		}
	}

	const FArcTacticsDebugActiveAction& Active = Snapshot.ActiveAction;
	if (Active.bSet)
	{
		const FString TargetLocation = Active.bHasTargetLocation
			? FString::Printf(TEXT(" at %s"), *Active.TargetLocation.ToCompactString())
			: FString();
		OutLines.Add(FString::Printf(
			TEXT("  active package=%s name=%s node=%s serial=%u source=%s target=%s%s priority=%d score=%.4f entered=%s lane=%s blocks=%s wake=%.3f step=%d/%d:%s (%s) hold=[%s:%u]"),
			*Active.Package, *Active.Name.ToString(), *Active.TypeName, Active.Serial,
			*SourceText(Active.bFromSelector, Active.SourceIndex),
			*IdentityText(Active.Target), *TargetLocation,
			Active.Priority, Active.Score, BoolText(Active.bEntered), LaneText(Active.Lane),
			BoolText(Active.bBlocksDecision), Active.WakeTime,
			Active.StepIndex, Active.StepCount, *Active.StepName.ToString(), *Active.StepTypeName,
			Active.bHoldOwned ? TEXT("owned") : TEXT("free"), Active.HoldActionID));
	}
	else
	{
		OutLines.Add(TEXT("  active=None"));
	}
	const FArcTacticsDebugEndedAction& Ended = Snapshot.LastEndedAction;
	if (Ended.bSet)
	{
		OutLines.Add(FString::Printf(
			TEXT("  last_end=%s/%s status=%s reason=%s failure=%s t=%.3f"),
			*Ended.Package, *Ended.Name.ToString(), EndStatusText(Ended.Status), EndReasonText(Ended.Reason),
			*Ended.FailureReason.ToString(), Ended.Time));
	}
	else
	{
		OutLines.Add(TEXT("  last_end=None"));
	}

	FString HistoryText;
	for (const FArcTacticsDebugHistoryEntry& Entry : Snapshot.History)
	{
		HistoryText += FString::Printf(TEXT(" %s(%.1fs)"), *Entry.Tag.ToString(), Entry.Age);
	}
	OutLines.Add(FString::Printf(
		TEXT("  history newest first:%s"), HistoryText.IsEmpty() ? TEXT(" None") : *HistoryText));
	if (!Snapshot.bHasMemory)
	{
		OutLines.Add(TEXT("  memory=None"));
	}
	else
	{
		OutLines.Add(FString::Printf(TEXT("  memory entries=%d"), Snapshot.Memory.Num()));
		for (const FArcTacticsDebugMemoryEntry& Entry : Snapshot.Memory)
		{
			OutLines.Add(FString::Printf(
				TEXT("    memory %s (%s) %s"), *Entry.Key.ToString(), *Entry.TypeName, *Entry.ValueText));
		}
	}

	const FArcTacticsDebugLastDecision& Decision = Snapshot.LastDecision;
	if (!Decision.bValid)
	{
		OutLines.Add(TEXT("  last decision=None"));
		return;
	}
	OutLines.Add(FString::Printf(
		TEXT("  last decision generation=%u time=%.3f age=%.1fs status=%s diagnostic=%s candidates=%d"),
		Decision.DecisionGeneration, Decision.Time, Snapshot.Now - Decision.Time,
		DecisionStatusText(Decision.Status), *Decision.Diagnostic.ToString(), Decision.Candidates.Num()));
	const FArcTacticsDebugCandidate* Selected = Decision.Candidates.FindByPredicate(
		[](const FArcTacticsDebugCandidate& Candidate) -> bool
		{
			return Candidate.bSelected;
		});
	if (Selected)
	{
		OutLines.Add(FString::Printf(
			TEXT("  selected candidate[%d] package=%s name=%s source=%s item=%d target=%s score=%.4f"),
			Selected->Order, *Selected->Package, *Selected->Name.ToString(),
			*SourceText(Selected->bFromSelector, Selected->SourceIndex), Selected->ItemIndex,
			*IdentityText(Selected->Target), Selected->Score));
	}
	else
	{
		OutLines.Add(FString::Printf(TEXT("  selected=None order=%d"), Decision.SelectedOrder));
	}
	for (const FArcTacticsDebugCandidate& Candidate : Decision.Candidates)
	{
		const FString Priority = Candidate.bHasPriority
			? FString::Printf(TEXT("%d"), Candidate.Priority)
			: FString(TEXT("unresolved"));
		OutLines.Add(FString::Printf(
			TEXT("    candidate[%d] package=%s name=%s source=%s item=%d target=%s priority=%s score=%.4f outcome=%s reason=%s selected=%s"),
			Candidate.Order, *Candidate.Package, *Candidate.Name.ToString(),
			*SourceText(Candidate.bFromSelector, Candidate.SourceIndex), Candidate.ItemIndex,
			*IdentityText(Candidate.Target), *Priority, Candidate.Score,
			OutcomeText(Candidate.Outcome), *Candidate.Reason.ToString(), BoolText(Candidate.bSelected)));
		for (int32 ConsiderationIndex = 0; ConsiderationIndex < Candidate.Considerations.Num(); ++ConsiderationIndex)
		{
			const FArcTacticsDebugConsideration& Consideration = Candidate.Considerations[ConsiderationIndex];
			const TCHAR* Break = ConsiderationIndex == Candidate.VetoIndex
				? TEXT(" veto")
				: (ConsiderationIndex == Candidate.EarlyOutIndex ? TEXT(" early-out") : TEXT(""));
			if (Consideration.bScored)
			{
				OutLines.Add(FString::Printf(
					TEXT("      %s raw=%.4f curved=%.4f%s"),
					*Consideration.Name.ToString(), Consideration.Raw, Consideration.Curved, Break));
			}
			else
			{
				OutLines.Add(FString::Printf(
					TEXT("      %s not scored%s"), *Consideration.Name.ToString(), Break));
			}
		}
	}
}
