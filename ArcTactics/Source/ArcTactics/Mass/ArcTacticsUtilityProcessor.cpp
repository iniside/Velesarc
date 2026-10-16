// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcTacticsUtilityProcessor.h"

#include "ArcAISignals.h"
#include "ArcTacticsSignals.h"
#include "Decision/ArcTacticsConfig.h"
#include "Debugger/ArcTacticsTrace.h"
#include "Decision/ArcTacticsDecision.h"
#include "Engine/World.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassSignalSubsystem.h"
#include "Navigation/ArcPathFollow.h"
#include "Perception/ArcMassSightPerception.h"
#include "Teams/ArcMassTeamFragment.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsUtilityProcessor)

namespace ArcTacticsUtilityProcessorPrivate
{
	bool HasSignal(
		FMassSignalNameLookup& EntitySignals,
		const FMassEntityHandle Entity,
		const FName Signal)
	{
		TArray<FName> Signals;
		EntitySignals.GetSignalsForEntity(Entity, Signals);
		return Signals.Contains(Signal);
	}

	FArcTacticsTaskContext MakeTaskContext(
		UArcTacticsInstanceSubsystem& Instances,
		FMassEntityManager& EntityManager,
		const FArcTacticsInstanceSlot& Slot,
		const FArcTacticsInstanceHandle Handle,
		const float DeltaTime)
	{
		FArcTacticsTaskContext Context;
		Context.World = Instances.GetWorld();
		Context.EntityManager = &EntityManager;
		Context.Entity = Slot.Entity;
		Context.InstanceSubsystem = &Instances;
		Context.InstanceHandle = Handle;
		Context.AsyncCompletion = FArcTacticsTaskAsyncCompletion::CreateUObject(
			&Instances, &UArcTacticsInstanceSubsystem::CompleteTaskQuery);
		Context.DecisionGeneration = Slot.DecisionGeneration;
		Context.DeltaTime = DeltaTime;
		return Context;
	}

	FArcTQSFunctionContext MakeFunctionContext(
		UArcTacticsInstanceSubsystem& Instances,
		FMassEntityManager& EntityManager,
		const FArcTacticsInstanceSlot& Slot)
	{
		FArcTQSFunctionContext FunctionContext;
		FunctionContext.EntityManager = &EntityManager;
		FunctionContext.World = Instances.GetWorld();
		FunctionContext.Self = Slot.Entity;
		return FunctionContext;
	}

	FArcTacticsInstanceSlot* ResolveSameAction(
		UArcTacticsInstanceSubsystem& Instances,
		const FArcTacticsInstanceHandle Handle,
		const FMassEntityHandle Entity,
		const FArcTacticsPackage* Package,
		const uint32 PackageSerial,
		const uint32 ActionSerial)
	{
		FArcTacticsInstanceSlot* Slot = Instances.Resolve(Handle);
		return Slot && Slot->Entity == Entity
			&& Slot->ActiveAction.IsSet() && Slot->ActiveAction.Serial == ActionSerial
			&& ArcTacticsInstances::ResolveActivePackage(*Slot) == Package
			&& Package->Serial == PackageSerial
			? Slot
			: nullptr;
	}

	void ScheduleDecisionContinuation(
		UArcTacticsInstanceSubsystem& Instances,
		const FMassEntityHandle Entity,
		const float DeltaTime)
	{
		UWorld* World = Instances.GetWorld();
		UMassSignalSubsystem* Signals = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
		if (Signals)
		{
			Signals->DelaySignalEntity(
				UE::ArcTactics::Signals::TaskWakeup,
				Entity,
				FMath::Max(DeltaTime, KINDA_SMALL_NUMBER));
		}
	}

	void ScheduleActionWake(
		UArcTacticsInstanceSubsystem& Instances,
		const FArcTacticsInstanceHandle Handle)
	{
		FArcTacticsInstanceSlot* Slot = Instances.Resolve(Handle);
		if (!Slot || !Slot->ActiveAction.IsSet())
		{
			return;
		}
		FArcTacticsActionView ActionView;
		const FArcTacticsActionNode* Action = ArcTacticsInstances::ResolveActiveAction(*Slot, ActionView);
		UWorld* World = Instances.GetWorld();
		UMassSignalSubsystem* Signals = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
		if (!Action || !Signals || !World)
		{
			return;
		}
		const FMassEntityHandle Entity = Slot->Entity;
		const uint32 ActionSerial = Slot->ActiveAction.Serial;
		const double WakeTime = Action->GetNextWakeTime(ActionView);
		Slot = Instances.Resolve(Handle);
		if (!Slot || Slot->Entity != Entity || !Slot->ActiveAction.IsSet()
			|| Slot->ActiveAction.Serial != ActionSerial)
		{
			return;
		}
		if (Slot->ActionWakeSerial == ActionSerial
			&& Slot->ActionWakeTime <= World->GetTimeSeconds())
		{
			return;
		}
		if (!FMath::IsFinite(WakeTime) || WakeTime <= 0.0)
		{
			Slot->ActionWakeSerial = 0;
			Slot->ActionWakeTime = 0.0;
			return;
		}
		Slot->ActionWakeSerial = Slot->ActiveAction.Serial;
		Slot->ActionWakeTime = WakeTime;
		Signals->DelaySignalEntity(
			UE::ArcTactics::Signals::ActionWakeup,
			Entity,
			static_cast<float>(FMath::Max(0.0, WakeTime - World->GetTimeSeconds())));
	}

	EArcTacticsActionEndStatus ToEndStatus(const EArcTacticsActionStatus Status)
	{
		return Status == EArcTacticsActionStatus::Succeeded
			? EArcTacticsActionEndStatus::Succeeded
			: EArcTacticsActionEndStatus::Failed;
	}

	void ClearCompletedAction(FArcTacticsInstanceSlot& Slot)
	{
		Slot.ClearActiveAction();
		Slot.bActionEntered = false;
		Slot.ActionWakeSerial = 0;
		Slot.ActionWakeTime = 0.0;
		Slot.bDecisionRequested = true;
	}

	void ScheduleRequestedDecision(UArcTacticsInstanceSubsystem& Instances, FArcTacticsInstanceSlot& Slot)
	{
		if (!Slot.bDecisionRequested || Slot.DecisionStatus == EArcTacticsDecisionStatus::Running
			|| !Slot.HasActivePackage())
		{
			return;
		}
		UWorld* World = Instances.GetWorld();
		UMassSignalSubsystem* Signals = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
		if (!Signals)
		{
			return;
		}
		const double Now = World->GetTimeSeconds();
		if (Now >= Slot.NextDecisionTime)
		{
			Signals->SignalEntity(UE::ArcTactics::Signals::DecisionDue, Slot.Entity);
			return;
		}
		if (Slot.ScheduledDecisionTime > Now)
		{
			return;
		}
		const float Delay = static_cast<float>(Slot.NextDecisionTime - Now);
		Slot.ScheduledDecisionTime = Now + Delay;
		Signals->DelaySignalEntity(UE::ArcTactics::Signals::DecisionDue, Slot.Entity, Delay);
	}

	bool IsFailedDecision(const EArcTacticsDecisionStatus Status, const FArcTacticsInstanceSlot& Slot)
	{
		static const FName SelectorItemCaptureFailed(TEXT("SelectorItemCaptureFailed"));
		return Status == EArcTacticsDecisionStatus::Failed
			|| (Status == EArcTacticsDecisionStatus::Succeeded && !Slot.HasSelectedAction()
				&& Slot.Diagnostic == SelectorItemCaptureFailed);
	}
}

void UArcTacticsUtilityProcessor::RunDecision(
	UArcTacticsInstanceSubsystem& Instances,
	FMassEntityManager& EntityManager,
	const FArcTacticsInstanceHandle Handle,
	const float DeltaTime,
	const ArcPathFollow::FContext* PathContext)
{
	using namespace ArcTacticsUtilityProcessorPrivate;
	FArcTacticsInstanceSlot* Slot = Instances.Resolve(Handle);
	UWorld* World = Instances.GetWorld();
	if (!Slot || !World || !EntityManager.IsEntityActive(Slot->Entity))
	{
		return;
	}
	FArcTacticsExecutionScope Scope(Instances, Handle);
	const FMassEntityHandle Entity = Slot->Entity;
	const bool bPendingDecision = Slot->DecisionStatus == EArcTacticsDecisionStatus::Running;
	if (!bPendingDecision)
	{
		if (!Slot->bDecisionRequested)
		{
			return;
		}
		if (!Slot->HasActivePackage())
		{
			Slot->bDecisionRequested = false;
			Slot->DecisionStatus = EArcTacticsDecisionStatus::Idle;
			return;
		}
		if (World->GetTimeSeconds() < Slot->NextDecisionTime)
		{
			ScheduleRequestedDecision(Instances, *Slot);
			return;
		}
		FArcTacticsActionView ActiveView;
		const FArcTacticsActionNode* ActiveAction =
			ArcTacticsInstances::ResolveActiveAction(*Slot, ActiveView);
		const bool bBlocksDecision = ActiveAction && ActiveAction->BlocksDecision(ActiveView);
		Slot = Instances.Resolve(Handle);
		if (!Slot || Slot->Entity != Entity)
		{
			return;
		}
		if (bBlocksDecision)
		{
			return;
		}
		Slot->bDecisionRequested = false;
	}

	FArcTacticsTaskContext TaskContext = MakeTaskContext(Instances, EntityManager, *Slot, Handle, DeltaTime);
	const EArcTacticsDecisionStatus Status = bPendingDecision
		? ArcTacticsDecision::TickDecision(*Slot, TaskContext)
		: ArcTacticsDecision::BeginDecision(*Slot, TaskContext);
	Slot = Instances.Resolve(Handle);
	if (!Slot || Slot->Entity != Entity || Status == EArcTacticsDecisionStatus::Idle)
	{
		return;
	}
	if (Status == EArcTacticsDecisionStatus::Running)
	{
		ScheduleDecisionContinuation(Instances, Entity, DeltaTime);
		return;
	}
	const bool bDecisionFailed = IsFailedDecision(Status, *Slot);

	if (Status == EArcTacticsDecisionStatus::Succeeded && Slot->HasSelectedAction())
	{
		if (Slot->ActiveAction.IsSet())
		{
			FArcTacticsPackage* ReplacedPackage = ArcTacticsInstances::ResolveActivePackage(*Slot);
			const uint32 ReplacedPackageSerial = Slot->ActiveAction.PackageSerial;
			const uint32 ReplacedSerial = Slot->ActiveAction.Serial;
			FArcTacticsActionView ActiveView;
			const FArcTacticsActionNode* ActiveAction =
				ArcTacticsInstances::ResolveActiveAction(*Slot, ActiveView);
			if (ActiveAction)
			{
				FArcTacticsActionContext ActionContext =
					ArcTacticsInstances::MakeActionContext(
						Instances, EntityManager, *Slot, Handle, PathContext);
				ActiveAction->Cancel(
					ActionContext, ActiveView.ActionData, EArcTacticsActionCancelMode::PreserveHold);
				Slot = ResolveSameAction(
					Instances, Handle, Entity, ReplacedPackage, ReplacedPackageSerial, ReplacedSerial);
				if (!Slot)
				{
					return;
				}
			}
			if (ReplacedPackage)
			{
				ArcTacticsInstances::RecordEndedAction(
					*Slot, *ReplacedPackage, *World, EntityManager,
					EArcTacticsActionEndStatus::Cancelled, EArcTacticsActionEndReason::Replaced, NAME_None);
			}
			Slot->ClearActiveAction();
			Slot->bActionEntered = false;
		}

		Slot->ActivateSelectedAction();
		TRACE_ARCTACTICS_ACTION_ENTERED(World, &EntityManager, *Slot);
		FArcTacticsPackage* Package = ArcTacticsInstances::ResolveActivePackage(*Slot);
		FArcTacticsActionView ActionView;
		const FArcTacticsActionNode* Action =
			ArcTacticsInstances::ResolveActiveAction(*Slot, ActionView);
		if (!Action || !Package)
		{
			ArcTacticsInstances::RecordLastEndedAction(
				*Slot, World, EArcTacticsActionEndStatus::Failed, EArcTacticsActionEndReason::Unresolved, NAME_None);
			TRACE_ARCTACTICS_ACTION_ENDED(World, &EntityManager, *Slot, EArcTacticsActionEndStatus::Failed,
				EArcTacticsActionEndReason::Unresolved, false, false);
			ClearCompletedAction(*Slot);
		}
		else
		{
			const uint32 PackageSerial = Package->Serial;
			const uint32 ActionSerial = Slot->ActiveAction.Serial;
			FArcTacticsActionContext ActionContext =
				ArcTacticsInstances::MakeActionContext(
					Instances, EntityManager, *Slot, Handle, PathContext);
			EArcTacticsActionStatus ActionStatus = EArcTacticsActionStatus::Failed;
			EArcTacticsActionEndReason EndReason = EArcTacticsActionEndReason::InputsFailed;
			if (ArcTacticsDecision::ApplyActiveActionInputs(
				*Slot, *Package, MakeFunctionContext(Instances, EntityManager, *Slot)))
			{
				EndReason = EArcTacticsActionEndReason::EnterEnded;
				ActionStatus = Action->Enter(ActionContext, ActionView.ActionData);
				Slot = ResolveSameAction(Instances, Handle, Entity, Package, PackageSerial, ActionSerial);
				if (!Slot)
				{
					return;
				}
				if (ActionStatus != EArcTacticsActionStatus::Failed
					&& !ArcTacticsDecision::PublishActiveActionOutputs(
						*Slot, *Package, MakeFunctionContext(Instances, EntityManager, *Slot)))
				{
					ActionStatus = EArcTacticsActionStatus::Failed;
				}
			}
			if (ActionStatus != EArcTacticsActionStatus::Failed
				&& Slot->ActiveAction.Parameters.CooldownStart == EArcTacticsCooldownStart::OnEnter)
			{
				ArcTacticsInstances::StampCooldown(*Slot, *Package, *World);
			}
			Slot->bActionEntered = ActionStatus == EArcTacticsActionStatus::Running;
			if (ActionStatus == EArcTacticsActionStatus::Failed)
			{
				FArcTacticsActionView FailedView;
				const FArcTacticsActionNode* FailedAction =
					ArcTacticsInstances::ResolveActiveAction(*Slot, FailedView);
				if (FailedAction)
				{
					FArcTacticsActionContext FailedContext =
						ArcTacticsInstances::MakeActionContext(
							Instances, EntityManager, *Slot, Handle, PathContext);
					FailedAction->Cancel(
						FailedContext, FailedView.ActionData, EArcTacticsActionCancelMode::PreserveHold);
					Slot = ResolveSameAction(Instances, Handle, Entity, Package, PackageSerial, ActionSerial);
					if (!Slot)
					{
						return;
					}
				}
			}
			if (ActionStatus != EArcTacticsActionStatus::Running)
			{
				ArcTacticsInstances::RecordEndedAction(
					*Slot, *Package, *World, EntityManager, ToEndStatus(ActionStatus), EndReason,
					ActionStatus == EArcTacticsActionStatus::Failed ? ActionContext.FailureReason : NAME_None);
				ClearCompletedAction(*Slot);
			}
		}
	}

	Slot->NextDecisionTime = World->GetTimeSeconds() + Slot->Settings.DecisionInterval;
	if (bDecisionFailed && !Slot->ActiveAction.IsSet())
	{
		Slot->bDecisionRequested = true;
	}
	ScheduleRequestedDecision(Instances, *Slot);
	ScheduleActionWake(Instances, Handle);
}

void UArcTacticsUtilityProcessor::TickAction(
	UArcTacticsInstanceSubsystem& Instances,
	FMassEntityManager& EntityManager,
	const FArcTacticsInstanceHandle Handle,
	const EArcTacticsActionTickLane Lane,
	const bool bExternalWake,
	const ArcPathFollow::FContext* PathContext)
{
	using namespace ArcTacticsUtilityProcessorPrivate;
	FArcTacticsInstanceSlot* Slot = Instances.Resolve(Handle);
	UWorld* World = Instances.GetWorld();
	if (!Slot || !World || !Slot->ActiveAction.IsSet()
		|| !EntityManager.IsEntityActive(Slot->Entity))
	{
		return;
	}
	FArcTacticsExecutionScope Scope(Instances, Handle);
	const FMassEntityHandle Entity = Slot->Entity;
	const uint32 ActionSerial = Slot->ActiveAction.Serial;
	FArcTacticsPackage* Package = ArcTacticsInstances::ResolveActivePackage(*Slot);
	FArcTacticsActionView ActionView;
	const FArcTacticsActionNode* Action = ArcTacticsInstances::ResolveActiveAction(*Slot, ActionView);
	if (!Action || !Package)
	{
		return;
	}
	const uint32 PackageSerial = Package->Serial;
	const EArcTacticsActionTickLane ActionLane = Action->GetTickLane(ActionView);
	Slot = ResolveSameAction(Instances, Handle, Entity, Package, PackageSerial, ActionSerial);
	if (!Slot || ActionLane != Lane)
	{
		return;
	}
	if (Lane == EArcTacticsActionTickLane::Utility && !bExternalWake
		&& (Slot->ActionWakeSerial != ActionSerial
			|| World->GetTimeSeconds() < Slot->ActionWakeTime))
	{
		return;
	}
	Slot->ActionWakeSerial = 0;
	Slot->ActionWakeTime = 0.0;
	ActionView = FArcTacticsActionView();
	Action = ArcTacticsInstances::ResolveActiveAction(*Slot, ActionView);
	if (!Action)
	{
		return;
	}
	FArcTacticsActionContext ActionContext = ArcTacticsInstances::MakeActionContext(
		Instances, EntityManager, *Slot, Handle, PathContext);
	EArcTacticsActionStatus Status = EArcTacticsActionStatus::Failed;
	if (ArcTacticsDecision::ApplyActiveActionInputs(
		*Slot, *Package, MakeFunctionContext(Instances, EntityManager, *Slot)))
	{
		Status = Action->Tick(ActionContext, ActionView.ActionData);
		Slot = ResolveSameAction(Instances, Handle, Entity, Package, PackageSerial, ActionSerial);
		if (!Slot)
		{
			return;
		}
		if (Status != EArcTacticsActionStatus::Failed
			&& !ArcTacticsDecision::PublishActiveActionOutputs(
				*Slot, *Package, MakeFunctionContext(Instances, EntityManager, *Slot)))
		{
			Status = EArcTacticsActionStatus::Failed;
		}
	}
	if (Status == EArcTacticsActionStatus::Running)
	{
		ScheduleActionWake(Instances, Handle);
		Slot = ResolveSameAction(Instances, Handle, Entity, Package, PackageSerial, ActionSerial);
		FArcTacticsActionView CurrentView;
		const FArcTacticsActionNode* CurrentAction = Slot
			? ArcTacticsInstances::ResolveActiveAction(*Slot, CurrentView) : nullptr;
		if (!CurrentAction)
		{
			return;
		}
		const bool bBlocksDecision = CurrentAction->BlocksDecision(CurrentView);
		Slot = Instances.Resolve(Handle);
		if (Slot && Slot->Entity == Entity && !bBlocksDecision)
		{
			ScheduleRequestedDecision(Instances, *Slot);
		}
		return;
	}
	if (Status == EArcTacticsActionStatus::Failed)
	{
		FArcTacticsActionContext CancelContext = ArcTacticsInstances::MakeActionContext(
			Instances, EntityManager, *Slot, Handle, PathContext);
		Action->Cancel(CancelContext, ActionView.ActionData, EArcTacticsActionCancelMode::PreserveHold);
		Slot = ResolveSameAction(Instances, Handle, Entity, Package, PackageSerial, ActionSerial);
		if (!Slot)
		{
			return;
		}
	}
	ArcTacticsInstances::RecordEndedAction(
		*Slot, *Package, *World, EntityManager, ToEndStatus(Status), EArcTacticsActionEndReason::Completed,
		Status == EArcTacticsActionStatus::Failed ? ActionContext.FailureReason : NAME_None);
	ClearCompletedAction(*Slot);
	ScheduleRequestedDecision(Instances, *Slot);
}

UArcTacticsUtilityProcessor::UArcTacticsUtilityProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcMassSightPerceptionProcessor::StaticClass()->GetFName());
}

void UArcTacticsUtilityProcessor::InitializeInternal(
	UObject& Owner,
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	if (UWorld* World = Owner.GetWorld())
	{
		if (UMassSignalSubsystem* Signals = World->GetSubsystem<UMassSignalSubsystem>())
		{
			SubscribeToSignal(*Signals, UE::ArcTactics::Signals::DecisionDue);
			SubscribeToSignal(*Signals, UE::ArcTactics::Signals::TaskWakeup);
			SubscribeToSignal(*Signals, UE::ArcTactics::Signals::ActionWakeup);
			SubscribeToSignal(*Signals, UE::ArcTactics::Signals::AbilityWakeup);
			SubscribeToSignal(*Signals, UE::ArcAI::Signals::PerceptionChanged);
		}
	}
}

void UArcTacticsUtilityProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>&)
{
	EntityQuery.AddTagRequirement<FArcTacticsTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcTacticsInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FAgentHeightFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassNavMeshShortPathFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassNavMeshCachedPathFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcMassTeamFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcTacticsConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::Optional);
	EntityQuery.AddSubsystemRequirement<UArcTacticsInstanceSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UArcTacticsUtilityProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = Context.GetWorld();
	UArcTacticsInstanceSubsystem* Instances =
		World ? World->GetSubsystem<UArcTacticsInstanceSubsystem>() : nullptr;
	if (!World || !Instances)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[World, Instances, &EntityManager, &EntitySignals](FMassExecutionContext& ChunkContext)
		{
			const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
			const FMassMovementParameters* MovementParameters =
				ChunkContext.GetConstSharedFragmentPtr<FMassMovementParameters>();
			TArrayView<FArcTacticsInstanceFragment> InstanceFragments =
				ChunkContext.GetMutableFragmentView<FArcTacticsInstanceFragment>();
			TArrayView<FTransformFragment> Transforms =
				ChunkContext.GetMutableFragmentView<FTransformFragment>();
			const TConstArrayView<FAgentRadiusFragment> Radii =
				ChunkContext.GetFragmentView<FAgentRadiusFragment>();
			const TConstArrayView<FAgentHeightFragment> Heights =
				ChunkContext.GetFragmentView<FAgentHeightFragment>();
			const TConstArrayView<FMassDesiredMovementFragment> DesiredMovements =
				ChunkContext.GetFragmentView<FMassDesiredMovementFragment>();
			TArrayView<FMassMoveTargetFragment> MoveTargets =
				ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
			TArrayView<FMassNavMeshShortPathFragment> ShortPaths =
				ChunkContext.GetMutableFragmentView<FMassNavMeshShortPathFragment>();
			TArrayView<FMassNavMeshCachedPathFragment> CachedPaths =
				ChunkContext.GetMutableFragmentView<FMassNavMeshCachedPathFragment>();
			const TConstArrayView<FArcMassTeamFragment> Teams =
				ChunkContext.GetFragmentView<FArcMassTeamFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				FArcTacticsInstanceFragment& InstanceFragment = InstanceFragments[EntityIt];
				const bool bDecisionSignal = ArcTacticsUtilityProcessorPrivate::HasSignal(
					EntitySignals, Entity, UE::ArcTactics::Signals::DecisionDue);
				const bool bActionSignal = ArcTacticsUtilityProcessorPrivate::HasSignal(
					EntitySignals, Entity, UE::ArcTactics::Signals::ActionWakeup);
				const bool bTaskSignal = ArcTacticsUtilityProcessorPrivate::HasSignal(
					EntitySignals, Entity, UE::ArcTactics::Signals::TaskWakeup);
				const bool bAbilitySignal = ArcTacticsUtilityProcessorPrivate::HasSignal(
					EntitySignals, Entity, UE::ArcTactics::Signals::AbilityWakeup);
				const bool bPerceptionSignal = ArcTacticsUtilityProcessorPrivate::HasSignal(
					EntitySignals, Entity, UE::ArcAI::Signals::PerceptionChanged);
				if (bDecisionSignal || bPerceptionSignal)
				{
					if (FArcTacticsInstanceSlot* Slot = Instances->Resolve(InstanceFragment.Handle))
					{
						Slot->bDecisionRequested = true;
					}
				}
				if (bDecisionSignal || bTaskSignal || bPerceptionSignal)
				{
					const bool bHasPathContext = MovementParameters
						&& !Transforms.IsEmpty() && !Radii.IsEmpty() && !Heights.IsEmpty()
						&& !DesiredMovements.IsEmpty() && !MoveTargets.IsEmpty()
						&& !ShortPaths.IsEmpty() && !CachedPaths.IsEmpty();
					if (bHasPathContext)
					{
						const ArcPathFollow::FContext PathContext{
							*World, Entity, Transforms[EntityIt], Radii[EntityIt], Heights[EntityIt],
							DesiredMovements[EntityIt], *MovementParameters, MoveTargets[EntityIt],
							ShortPaths[EntityIt], CachedPaths[EntityIt],
							Teams.IsEmpty() ? INDEX_NONE : Teams[EntityIt].TeamId};
						RunDecision(
							*Instances, EntityManager, InstanceFragment.Handle, DeltaTime, &PathContext);
					}
					else
					{
						RunDecision(
							*Instances, EntityManager, InstanceFragment.Handle, DeltaTime, nullptr);
					}
				}
				if (bActionSignal || bAbilitySignal)
				{
					TickAction(
						*Instances, EntityManager, InstanceFragment.Handle,
						EArcTacticsActionTickLane::Utility, bAbilitySignal, nullptr);
				}
			}
		});
}
