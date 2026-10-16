// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcBodyUtilityProcessor.h"

#include "ArcNeedSignals.h"
#include "Engine/World.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectHandler.h"
#include "MassSubsystemAccess.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyCommandLogic.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBodyUtilityConsiderations.h"
#include "Sim/ArcBodyUtilityFragments.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcEatLogic.h"
#include "Sim/ArcGovernorProcessor.h"
#include "Sim/ArcMatchProcessor.h"
#include "Sim/ArcServiceLogic.h"
#include "Sim/ArcSettlementCenter.h"
#include "Sim/ArcWorldSimStores.h"
#include "Sim/ArcWorldSimTags.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"
#include "UtilityAI/ArcUtilityScoringSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcBodyUtilityProcessor)

namespace ArcBodyUtilityProcessorPrivate
{
const FName ScheduledEvaluateSignal = FName(TEXT("ArcWorldSimBodyUtilityScheduledEvaluate"));
const FName IntentReadySignal = FName(TEXT("ArcWorldSimBodyUtilityIntentReady"));

bool HasSignal(
	FMassSignalNameLookup& EntitySignals,
	const FMassEntityHandle Entity,
	const FName Signal)
{
	TArray<FName> Signals;
	EntitySignals.GetSignalsForEntity(Entity, Signals);
	return Signals.Contains(Signal);
}

uint32 CurrentCommandSerial(const FArcBodyFragment& Body)
{
	return Body.Command.IsActive() ? Body.Command.Serial : 0;
}

void ScheduleEvaluation(
	FArcBodyUtilityIntentFragment& Intent,
	UMassSignalSubsystem& SignalSubsystem,
	const FMassEntityHandle Entity,
	const double Now,
	const double Deadline)
{
	const double ClampedDeadline = FMath::Max(Now, Deadline);
	if (Intent.ScoringRequestId != INDEX_NONE)
	{
		if (!Intent.bEvaluationDeferred || ClampedDeadline < Intent.NextEvaluationTime)
		{
			Intent.NextEvaluationTime = ClampedDeadline;
		}
		Intent.bEvaluationDeferred = true;
		return;
	}

	if (Intent.bEvaluationScheduled && Intent.NextEvaluationTime <= ClampedDeadline)
	{
		return;
	}

	Intent.NextEvaluationTime = ClampedDeadline;
	Intent.bEvaluationScheduled = true;
	SignalSubsystem.DelaySignalEntity(
		ScheduledEvaluateSignal,
		Entity,
		static_cast<float>(ClampedDeadline - Now));
}

void RequestEvaluationFromCurrentSignal(FArcBodyUtilityIntentFragment& Intent, const double Now)
{
	if (Intent.ScoringRequestId != INDEX_NONE)
	{
		if (!Intent.bEvaluationDeferred || Now < Intent.NextEvaluationTime)
		{
			Intent.NextEvaluationTime = Now;
		}
		Intent.bEvaluationDeferred = true;
		return;
	}

	Intent.NextEvaluationTime = Now;
	Intent.bEvaluationScheduled = true;
}

void ScheduleAfterIntent(
	FArcBodyUtilityIntentFragment& Intent,
	UMassSignalSubsystem& SignalSubsystem,
	const FMassEntityHandle Entity,
	const double Now,
	double Deadline)
{
	if (Intent.bEvaluationDeferred)
	{
		Deadline = FMath::Min(Deadline, Intent.NextEvaluationTime);
		Intent.bEvaluationDeferred = false;
	}
	ScheduleEvaluation(Intent, SignalSubsystem, Entity, Now, Deadline);
}

float ScoreForEntry(const FArcUtilityScoringInstance& Instance, const int32 EntryIndex)
{
	for (const FArcUtilityScoringInstance::FScoredPair& Pair : Instance.ScoredPairs)
	{
		if (Pair.EntryIndex == EntryIndex)
		{
			return Pair.Score;
		}
	}
	return 0.0f;
}

int32 FindOptionIndex(const FArcBodyUtilityConfigFragment& Config, const FGameplayTag GoalTag)
{
	for (int32 OptionIndex = 0; OptionIndex < Config.Options.Num(); ++OptionIndex)
	{
		if (Config.Options[OptionIndex].GoalTag == GoalTag)
		{
			return OptionIndex;
		}
	}
	return INDEX_NONE;
}

void GatherServices(FMassEntityQuery& Query, FMassExecutionContext& Context, TArray<ArcWorldSim::FServiceRecord>& Out)
{
	Query.ForEachEntityChunk(Context, [&Out](FMassExecutionContext& ChunkContext)
	{
		const FArcServiceConfigFragment& Config = ChunkContext.GetConstSharedFragment<FArcServiceConfigFragment>();
		const TConstArrayView<FTransformFragment> Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ArcWorldSim::FServiceRecord Record;
			Record.Entity = ChunkContext.GetEntity(EntityIt);
			Record.Location = Transforms[EntityIt].GetTransform().GetLocation();
			Record.SlotTags = Config.SlotTags;
			Out.Add(Record);
		}
	});
}

void GatherCenters(
	FMassEntityQuery& Query,
	FMassExecutionContext& Context,
	TArray<ArcWorldSim::FSettlementBound>& Out)
{
	Query.ForEachEntityChunk(Context, [&Out](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FArcSettlementCenterFragment> CenterFragments =
			ChunkContext.GetFragmentView<FArcSettlementCenterFragment>();
		const TConstArrayView<FTransformFragment> Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ArcWorldSim::FSettlementBound Bound;
			Bound.Entity = ChunkContext.GetEntity(EntityIt);
			Bound.Transform = Transforms[EntityIt].GetTransform();
			Bound.Extent = CenterFragments[EntityIt].Extent;
			Out.Add(Bound);
		}
	});
}

bool ResolveGoal(
	FMassEntityManager& EntityManager,
	USmartObjectSubsystem& SmartObjectSubsystem,
	const FGameplayTag Goal,
	const FMassEntityHandle PreferredWorkplace,
	const FMassEntityHandle Settlement,
	const FVector& Location,
	TConstArrayView<ArcWorldSim::FServiceRecord> Services,
	TConstArrayView<ArcWorldSim::FSettlementBound> Centers,
	FMassEntityHandle& OutTarget,
	FSmartObjectRequestResult& OutRequestResult)
{
	if (Goal == ArcWorldSimTags::Need_Hunger)
	{
		return ArcWorldSim::FindServicePlace(
			EntityManager,
			SmartObjectSubsystem,
			PreferredWorkplace,
			Settlement,
			Location,
			ArcWorldSimTags::Slot_Eat,
			Services,
			Centers,
			[&EntityManager](const FMassEntityHandle Service) -> bool
			{
				ArcWorldSim::FEatPortion Portion;
				return ArcWorldSim::FindEatPortion(EntityManager, Service, Portion);
			},
			OutTarget,
			OutRequestResult);
	}
	if (Goal == ArcWorldSimTags::Need_Fatigue)
	{
		return ArcWorldSim::FindServicePlace(
			EntityManager,
			SmartObjectSubsystem,
			FMassEntityHandle(),
			Settlement,
			Location,
			ArcWorldSimTags::Slot_Sleep,
			Services,
			Centers,
			[](const FMassEntityHandle Service) -> bool
			{
				return Service.IsValid();
			},
			OutTarget,
			OutRequestResult);
	}
	return false;
}

void ExecuteIntent(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	USmartObjectSubsystem& SmartObjectSubsystem,
	UMassSignalSubsystem& SignalSubsystem,
	const FMassEntityHandle Entity,
	FArcBodyFragment& Body,
	FArcBodyUtilityIntentFragment& Intent,
	FMassSmartObjectUserFragment& User,
	const FArcBodyUtilityConfigFragment& Config,
	const FArcSettlementMembershipFragment& Membership,
	const FArcWorkAssignmentFragment& WorkAssignment,
	const FArcGatherAssignmentFragment& GatherAssignment,
	const FVector& Location,
	TConstArrayView<ArcWorldSim::FServiceRecord> Services,
	TConstArrayView<ArcWorldSim::FSettlementBound> Centers,
	const double Now)
{
	if (!Intent.bIntentReady)
	{
		return;
	}
	Intent.bIntentReady = false;

	if (Intent.SelectedGoal == ArcWorldSimTags::Need_Continue)
	{
		ScheduleAfterIntent(Intent, SignalSubsystem, Entity, Now, Now + Config.NormalInterval);
		return;
	}

	if (Intent.ActiveCommandSerial != 0
		&& Body.Command.IsActive()
		&& Body.Command.Serial == Intent.ActiveCommandSerial
		&& Intent.ActiveGoal == Intent.SelectedGoal)
	{
		ScheduleAfterIntent(Intent, SignalSubsystem, Entity, Now, Now + Config.NormalInterval);
		return;
	}

	FArcUtilityContext UtilityContext;
	UtilityContext.QuerierEntity = Entity;
	UtilityContext.World = EntityManager.GetWorld();
	UtilityContext.EntityManager = &EntityManager;
	FArcUtilityTarget SelfTarget;
	SelfTarget.TargetType = EArcUtilityTargetType::Entity;
	SelfTarget.EntityHandle = Entity;
	const FArcUtilityConsideration_BodyContinuation Continuation;
	if (Continuation.Score(SelfTarget, UtilityContext).Score >= 1.0f)
	{
		ScheduleAfterIntent(Intent, SignalSubsystem, Entity, Now, Now + Config.NormalInterval);
		return;
	}

	if (Body.Command.IsActive()
		&& Intent.WinningScore < Intent.ContinueScore + Config.SwitchMargin)
	{
		ScheduleAfterIntent(Intent, SignalSubsystem, Entity, Now, Now + Config.NormalInterval);
		return;
	}

	const FMassEntityHandle PreferredWorkplace = WorkAssignment.Workplace.IsValid()
		? WorkAssignment.Workplace
		: GatherAssignment.Workplace;
	FMassEntityHandle Target;
	FSmartObjectRequestResult RequestResult{FSmartObjectHandle()};
	if (!ResolveGoal(
		EntityManager,
		SmartObjectSubsystem,
		Intent.SelectedGoal,
		PreferredWorkplace,
		Membership.Settlement,
		Location,
		Services,
		Centers,
		Target,
		RequestResult))
	{
		ScheduleAfterIntent(Intent, SignalSubsystem, Entity, Now, Now + Config.RetryInterval);
		return;
	}

	const uint32 ReplacedCommandSerial = CurrentCommandSerial(Body);
	if (ReplacedCommandSerial != 0 && !Body.Command.bInterruptible)
	{
		ScheduleAfterIntent(Intent, SignalSubsystem, Entity, Now, Now + Config.NormalInterval);
		return;
	}

	const FMassSmartObjectHandler Handler(Context, SmartObjectSubsystem, SignalSubsystem);
	const FSmartObjectClaimHandle ClaimHandle = Handler.ClaimSmartObject(Entity, User, RequestResult);
	if (!ClaimHandle.IsValid())
	{
		ScheduleAfterIntent(Intent, SignalSubsystem, Entity, Now, Now + Config.RetryInterval);
		return;
	}

	if (!ArcBodyCommand::TryAdoptClaimedSlot(
		EntityManager,
		Context,
		SmartObjectSubsystem,
		SignalSubsystem,
		Entity,
		Body,
		User,
		Target,
		RequestResult,
		ClaimHandle,
		ReplacedCommandSerial))
	{
		ScheduleAfterIntent(Intent, SignalSubsystem, Entity, Now, Now + Config.RetryInterval);
		return;
	}

	Intent.ActiveCommandSerial = Body.Command.Serial;
	Intent.ActiveGoal = Intent.SelectedGoal;
	ScheduleAfterIntent(Intent, SignalSubsystem, Entity, Now, Now + Config.NormalInterval);
}
}

UArcBodyUtilityProcessor::UArcBodyUtilityProcessor()
	: ServiceQuery(*this)
	, CenterQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteAfter.Add(UArcGovernorProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UArcMatchProcessor::StaticClass()->GetFName());
}

void UArcBodyUtilityProcessor::InitializeInternal(
	UObject& Owner,
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	if (SignalSubsystem)
	{
		SubscribeToSignal(*SignalSubsystem, ArcNeeds::Signals::UtilityEvaluate);
		SubscribeToSignal(*SignalSubsystem, ArcBodyUtilityProcessorPrivate::ScheduledEvaluateSignal);
		SubscribeToSignal(*SignalSubsystem, ArcBodyUtilityProcessorPrivate::IntentReadySignal);
	}
}

void UArcBodyUtilityProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<USmartObjectSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	EntityQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcSettlementMembershipFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcWorkAssignmentFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcGatherAssignmentFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcBodyUtilityConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddSparseRequirement<FArcBodyUtilityIntentFragment>(EMassFragmentPresence::All);

	ServiceQuery.Initialize(EntityManager);
	ServiceQuery.AddConstSharedRequirement<FArcServiceConfigFragment>(EMassFragmentPresence::All);
	ServiceQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ServiceQuery.RegisterWithProcessor(*this);

	CenterQuery.Initialize(EntityManager);
	CenterQuery.AddRequirement<FArcSettlementCenterFragment>(EMassFragmentAccess::ReadOnly);
	CenterQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	CenterQuery.RegisterWithProcessor(*this);
}

void UArcBodyUtilityProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetMutableSubsystem<USmartObjectSubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetMutableSubsystem<UMassSignalSubsystem>();
	UArcUtilityScoringSubsystem* ScoringSubsystem = World ? World->GetSubsystem<UArcUtilityScoringSubsystem>() : nullptr;
	if (!World || !SmartObjectSubsystem || !SignalSubsystem || !ScoringSubsystem)
	{
		return;
	}

	TArray<ArcWorldSim::FServiceRecord> Services;
	TArray<ArcWorldSim::FSettlementBound> Centers;
	ArcBodyUtilityProcessorPrivate::GatherServices(ServiceQuery, Context, Services);
	ArcBodyUtilityProcessorPrivate::GatherCenters(CenterQuery, Context, Centers);

	const double Now = World->GetTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context,
		[this, &EntityManager, &EntitySignals, ScoringSubsystem, SmartObjectSubsystem, SignalSubsystem,
			&Services, &Centers, World, Now](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			const TConstArrayView<FArcSettlementMembershipFragment> Memberships =
				ChunkContext.GetFragmentView<FArcSettlementMembershipFragment>();
			const TConstArrayView<FArcWorkAssignmentFragment> WorkAssignments =
				ChunkContext.GetFragmentView<FArcWorkAssignmentFragment>();
			const TConstArrayView<FArcGatherAssignmentFragment> GatherAssignments =
				ChunkContext.GetFragmentView<FArcGatherAssignmentFragment>();
			const TConstArrayView<FTransformFragment> Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			const FArcBodyUtilityConfigFragment& Config =
				ChunkContext.GetConstSharedFragment<FArcBodyUtilityConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcBodyUtilityIntentFragment* Intent =
					ChunkContext.GetMutableSparseElement<FArcBodyUtilityIntentFragment>(EntityIt);
				if (!Intent)
				{
					continue;
				}

				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				FArcBodyFragment& Body = Bodies[EntityIt];
				const bool bUtilityEvaluate = ArcBodyUtilityProcessorPrivate::HasSignal(
					EntitySignals, Entity, ArcNeeds::Signals::UtilityEvaluate);
				const bool bScheduledEvaluate = ArcBodyUtilityProcessorPrivate::HasSignal(
					EntitySignals, Entity, ArcBodyUtilityProcessorPrivate::ScheduledEvaluateSignal);
				const bool bIntentReady = ArcBodyUtilityProcessorPrivate::HasSignal(
					EntitySignals, Entity, ArcBodyUtilityProcessorPrivate::IntentReadySignal);

				if (Intent->ActiveCommandSerial != 0
					&& Body.LastCommandSerial == Intent->ActiveCommandSerial)
				{
					Intent->ActiveCommandSerial = 0;
					Intent->ActiveGoal = FGameplayTag();
					Intent->SelectedGoal = FGameplayTag();
					Intent->bIntentReady = false;
					if (bUtilityEvaluate || bScheduledEvaluate)
					{
						ArcBodyUtilityProcessorPrivate::RequestEvaluationFromCurrentSignal(*Intent, Now);
					}
					else
					{
						ArcBodyUtilityProcessorPrivate::ScheduleEvaluation(
							*Intent, *SignalSubsystem, Entity, Now, Now);
					}
				}

				if (bIntentReady)
				{
					ArcBodyUtilityProcessorPrivate::ExecuteIntent(
						EntityManager,
						ChunkContext,
						*SmartObjectSubsystem,
						*SignalSubsystem,
						Entity,
						Body,
						*Intent,
						Users[EntityIt],
						Config,
						Memberships[EntityIt],
						WorkAssignments[EntityIt],
						GatherAssignments[EntityIt],
						Transforms[EntityIt].GetTransform().GetLocation(),
						Services,
						Centers,
						Now);
				}

				if (bUtilityEvaluate)
				{
					ArcBodyUtilityProcessorPrivate::RequestEvaluationFromCurrentSignal(*Intent, Now);
				}

				if ((!bUtilityEvaluate && !bScheduledEvaluate)
					|| !Intent->bEvaluationScheduled
					|| Now + SMALL_NUMBER < Intent->NextEvaluationTime
					|| Intent->ScoringRequestId != INDEX_NONE)
				{
					continue;
				}

				Intent->bEvaluationScheduled = false;
				Intent->NextEvaluationTime = 0.0;
				Intent->bEvaluationDeferred = false;
				Intent->SubmittedBodyCommandSerial = ArcBodyUtilityProcessorPrivate::CurrentCommandSerial(Body);

				TArray<FArcUtilityEntry> Entries;
				Entries.Reserve(Config.Options.Num());
				for (const FArcBodyUtilityOption& Option : Config.Options)
				{
					Entries.Add(Option.Entry);
				}
				TArray<FArcUtilityTarget> Targets;
				FArcUtilityTarget Target;
				Target.TargetType = EArcUtilityTargetType::Entity;
				Target.EntityHandle = Entity;
				Targets.Add(Target);

				FArcUtilityContext UtilityContext;
				UtilityContext.QuerierEntity = Entity;
				UtilityContext.World = World;
				UtilityContext.EntityManager = &EntityManager;
				Intent->ScoringRequestId = ScoringSubsystem->SubmitRequest(
					MoveTemp(Entries),
					MoveTemp(Targets),
					UtilityContext,
					EArcUtilitySelectionMode::HighestScore,
					Config.MinScore,
					100.0f,
					FArcUtilityScoringFinished::CreateUObject(
						this,
						&UArcBodyUtilityProcessor::OnScoringFinished,
						Entity,
						Intent->SubmittedBodyCommandSerial));
				if (Intent->ScoringRequestId == INDEX_NONE)
				{
					Intent->SelectedGoal = FGameplayTag();
					ArcBodyUtilityProcessorPrivate::ScheduleEvaluation(
						*Intent, *SignalSubsystem, Entity, Now, Now);
				}
			}
		});
}

void UArcBodyUtilityProcessor::OnScoringFinished(
	FArcUtilityScoringInstance& CompletedInstance,
	const FMassEntityHandle Entity,
	const uint32 SubmittedBodyCommandSerial)
{
	FMassEntityManager* EntityManager = CompletedInstance.Context.EntityManager;
	if (!EntityManager || !EntityManager->IsEntityValid(Entity))
	{
		return;
	}

	const FMassEntityView EntityView(*EntityManager, Entity);
	const FArcBodyUtilityConfigFragment* Config =
		EntityView.GetConstSharedFragmentDataPtr<FArcBodyUtilityConfigFragment>();
	const int32 WinningEntryIndex = CompletedInstance.Result.WinningEntryIndex;
	const bool bHasWinner = Config
		&& CompletedInstance.Result.bSuccess
		&& Config->Options.IsValidIndex(WinningEntryIndex);
	const FGameplayTag WinningGoal = bHasWinner
		? Config->Options[WinningEntryIndex].GoalTag
		: FGameplayTag();
	const float WinningScore = bHasWinner
		? ArcBodyUtilityProcessorPrivate::ScoreForEntry(CompletedInstance, WinningEntryIndex)
		: 0.0f;
	const int32 ContinueIndex = Config
		? ArcBodyUtilityProcessorPrivate::FindOptionIndex(*Config, ArcWorldSimTags::Need_Continue)
		: INDEX_NONE;
	const float ContinueScore = ContinueIndex != INDEX_NONE
		? ArcBodyUtilityProcessorPrivate::ScoreForEntry(CompletedInstance, ContinueIndex)
		: 0.0f;
	const int32 RequestId = CompletedInstance.RequestId;

	EntityManager->Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[Entity, SubmittedBodyCommandSerial, RequestId, bHasWinner, WinningGoal, WinningScore, ContinueScore](
			FMassEntityManager& DeferredEntityManager)
		{
			if (!DeferredEntityManager.IsEntityValid(Entity))
			{
				return;
			}

			FStructView IntentView = DeferredEntityManager.GetMutableSparseElementDataForEntity(
				FArcBodyUtilityIntentFragment::StaticStruct(), Entity);
			FArcBodyUtilityIntentFragment* Intent = IntentView.GetPtr<FArcBodyUtilityIntentFragment>();
			FArcBodyFragment* Body = DeferredEntityManager.GetFragmentDataPtr<FArcBodyFragment>(Entity);
			UWorld* World = DeferredEntityManager.GetWorld();
			UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
			if (!Intent || Intent->ScoringRequestId != RequestId)
			{
				return;
			}

			Intent->ScoringRequestId = INDEX_NONE;
			const bool bStaleBodySerial = !Body
				|| Intent->SubmittedBodyCommandSerial != SubmittedBodyCommandSerial
				|| ArcBodyUtilityProcessorPrivate::CurrentCommandSerial(*Body) != SubmittedBodyCommandSerial;
			Intent->SubmittedBodyCommandSerial = 0;
			if (!SignalSubsystem)
			{
				Intent->bIntentReady = false;
				return;
			}

			const double Now = World->GetTimeSeconds();
			if (bStaleBodySerial || !bHasWinner)
			{
				Intent->SelectedGoal = FGameplayTag();
				Intent->bIntentReady = false;
				Intent->bEvaluationDeferred = false;
				Intent->bEvaluationScheduled = false;
				Intent->NextEvaluationTime = 0.0;
				ArcBodyUtilityProcessorPrivate::ScheduleEvaluation(
					*Intent, *SignalSubsystem, Entity, Now, Now);
				return;
			}

			Intent->SelectedGoal = WinningGoal;
			Intent->WinningScore = WinningScore;
			Intent->ContinueScore = ContinueScore;
			Intent->bIntentReady = true;
			SignalSubsystem->SignalEntity(ArcBodyUtilityProcessorPrivate::IntentReadySignal, Entity);
		});
}
