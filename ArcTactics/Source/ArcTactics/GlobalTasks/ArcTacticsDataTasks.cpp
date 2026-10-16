// Copyright Lukasz Baran. All Rights Reserved.

#include "GlobalTasks/ArcTacticsDataTasks.h"

#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryInstance.h"
#include "ArcTQSQuerySubsystem.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassCommonFragments.h"
#include "Memory/ArcMassMemory.h"
#include "Navigation/ArcPathFollow.h"
#include "Perception/ArcMassSightPerception.h"
#include "Perception/ArcSharedTargetKnowledge.h"
#include "Teams/ArcMassTeamFragment.h"

#if WITH_EDITOR
#include "PropertyBindingBindableStructDescriptor.h"
#include "PropertyBindingBindingCollection.h"
#include "PropertyBindingBindingCollectionOwner.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsDataTasks)

namespace ArcTacticsDataTasksPrivate
{
#if WITH_EDITOR
	FString EscapeRichText(const FStringView Text)
	{
		FString Result(Text);
		Result.ReplaceInline(TEXT("&"), TEXT("&amp;"));
		Result.ReplaceInline(TEXT("<"), TEXT("&lt;"));
		Result.ReplaceInline(TEXT(">"), TEXT("&gt;"));
		Result.ReplaceInline(TEXT("\""), TEXT("&quot;"));
		return Result;
	}

	FText Emphasize(const FText& Text, const bool bRichText)
	{
		const FString Escaped = EscapeRichText(Text.ToString());
		return bRichText
			? FText::FromString(FString::Printf(TEXT("<b>%s</>"), *Escaped))
			: Text;
	}

	FText GetBindingSource(
		const FGuid& NodeID,
		const FName PropertyName,
		const IPropertyBindingBindingCollectionOwner& BindingOwner,
		const bool bRichText)
	{
		const FPropertyBindingBindingCollection* Bindings = BindingOwner.GetEditorPropertyBindings();
		if (!Bindings)
		{
			return FText::GetEmpty();
		}

		const FPropertyBindingPath* SourcePath = Bindings->GetBindingSource(FPropertyBindingPath(NodeID, PropertyName));
		if (!SourcePath)
		{
			return FText::GetEmpty();
		}

		FString SourceName;
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> Descriptor;
		if (BindingOwner.GetBindableStructByID(SourcePath->GetStructID(), Descriptor))
		{
			SourceName = Descriptor.Get().Name.ToString();
		}
		const FString Path = SourcePath->ToString();
		if (!Path.IsEmpty())
		{
			if (!SourceName.IsEmpty())
			{
				SourceName += TEXT(".");
			}
			SourceName += Path;
		}
		return Emphasize(FText::FromString(SourceName), bRichText);
	}

	FText GetSourceOrField(
		const FGuid& NodeID,
		const FName PropertyName,
		const IPropertyBindingBindingCollectionOwner& BindingOwner,
		const bool bRichText)
	{
		const FText Source = GetBindingSource(NodeID, PropertyName, BindingOwner, bRichText);
		return Source.IsEmpty() ? Emphasize(FText::FromName(PropertyName), bRichText) : Source;
	}
#endif

	uint32 AdvanceNonZero(uint32& Value)
	{
		++Value;
		if (Value == 0)
		{
			++Value;
		}
		return Value;
	}

	bool HasEligibleTargetTags(
		FMassEntityManager& EntityManager,
		const FMassEntityHandle Target,
		const FGameplayTagContainer& RequiredTags,
		const FGameplayTagContainer& BlockedTags)
	{
		const FMassEntityView TargetView(EntityManager, Target);
		const FArcOwnedTagsFragment* OwnedTags = TargetView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		if (!RequiredTags.IsEmpty() && (!OwnedTags || !OwnedTags->GetTags().HasAll(RequiredTags)))
		{
			return false;
		}
		return BlockedTags.IsEmpty() || !OwnedTags || !OwnedTags->GetTags().HasAny(BlockedTags);
	}

	bool IsEligibleTarget(
		FMassEntityManager& EntityManager,
		const int32 ObserverTeamId,
		const FMassEntityHandle Target,
		const FArcTacticsCollectTargetsTaskInstanceData& Data)
	{
		const FArcMassTeamFragment* TargetTeam = EntityManager.GetFragmentDataPtr<FArcMassTeamFragment>(Target);
		const EArcTeamAttitude Attitude = ArcTeamAttitude::Resolve(
			ObserverTeamId, TargetTeam ? TargetTeam->TeamId : INDEX_NONE);
		return Attitude == Data.TargetAttitude
			&& HasEligibleTargetTags(EntityManager, Target, Data.RequiredTargetTags, Data.BlockedTargetTags);
	}

	void UpsertTarget(
		TArray<FArcTacticsTarget>& Targets,
		TArray<double>& SeenTimes,
		const FMassEntityHandle Entity,
		const FVector& Location,
		const double LastSeenTime)
	{
		for (int32 Index = 0; Index < Targets.Num(); ++Index)
		{
			if (Targets[Index].Entity == Entity)
			{
				if (LastSeenTime > SeenTimes[Index])
				{
					Targets[Index].LastLocation = Location;
					SeenTimes[Index] = LastSeenTime;
				}
				return;
			}
		}
		FArcTacticsTarget& Target = Targets.AddDefaulted_GetRef();
		Target.Entity = Entity;
		Target.LastLocation = Location;
		SeenTimes.Add(LastSeenTime);
	}

	EArcTacticsTaskStatus SubmitQuery(
		FArcTacticsTaskContext& Context,
		const FArcTQSQueryReference& Query,
		FArcTQSQueryContext& QueryContext,
		const FArcTQSSelectionOverride* SelectionOverride,
		FArcTacticsTaskQueryState& State)
	{
		UArcTQSQuerySubsystem* QuerySubsystem = Context.World
			? Context.World->GetSubsystem<UArcTQSQuerySubsystem>()
			: nullptr;
		if (!QuerySubsystem || !Context.EntityManager || !Query.IsValid()
			|| !Context.AsyncCompletion.IsBound() || !Context.InstanceSubsystem.IsValid()
			|| !Context.InstanceHandle.IsValid())
		{
			return EArcTacticsTaskStatus::Failed;
		}
		State.bCompleted = false;
		State.bSucceeded = false;
		AdvanceNonZero(State.RequestGeneration);

		QueryContext.QuerierEntity = Context.Entity;
		QueryContext.World = Context.World;
		QueryContext.EntityManager = Context.EntityManager;

		FArcTacticsAsyncRequestToken Token;
		Token.InstanceSubsystem = Context.InstanceSubsystem;
		Token.Entity = Context.Entity;
		Token.InstanceHandle = Context.InstanceHandle;
		Token.DecisionGeneration = Context.DecisionGeneration;
		Token.RequestGeneration = State.RequestGeneration;
		const FArcTacticsTaskAsyncCompletion Completion = Context.AsyncCompletion;
		FArcTQSQueryFinished Finished = FArcTQSQueryFinished::CreateLambda(
			[Completion, Token](FArcTQSQueryInstance& CompletedQuery)
			{
				Completion.ExecuteIfBound(Token, CompletedQuery);
			});
		State.QueryID = QuerySubsystem->RunQuery(Query, QueryContext, MoveTemp(Finished), 0, SelectionOverride);
		return State.QueryID == INDEX_NONE ? EArcTacticsTaskStatus::Failed : EArcTacticsTaskStatus::Running;
	}

	EArcTacticsTaskStatus GetQueryStatus(const FArcTacticsTaskQueryState& State)
	{
		return State.bCompleted
			? (State.bSucceeded ? EArcTacticsTaskStatus::Succeeded : EArcTacticsTaskStatus::Failed)
			: EArcTacticsTaskStatus::Running;
	}

	void CancelQuery(const FArcTacticsTaskContext& Context, FArcTacticsTaskQueryState& State)
	{
		UArcTQSQuerySubsystem* QuerySubsystem = Context.World
			? Context.World->GetSubsystem<UArcTQSQuerySubsystem>()
			: nullptr;
		if (QuerySubsystem && State.QueryID != INDEX_NONE)
		{
			QuerySubsystem->AbortQuery(State.QueryID);
		}
		State.QueryID = INDEX_NONE;
		AdvanceNonZero(State.RequestGeneration);
		State.bCompleted = true;
		State.bSucceeded = false;
	}

	bool CompleteQuery(
		const FArcTacticsTaskContext& Context,
		const FArcTacticsAsyncRequestToken& Token,
		const FArcTQSQueryInstance& CompletedQuery,
		FArcTacticsTaskQueryState& State,
		TArray<FVector>& OutPositions)
	{
		if (Token.InstanceSubsystem != Context.InstanceSubsystem
			|| Token.Entity != Context.Entity || Token.InstanceHandle != Context.InstanceHandle
			|| Token.DecisionGeneration != Context.DecisionGeneration
			|| Token.RequestGeneration != State.RequestGeneration
			|| CompletedQuery.QueryId != State.QueryID)
		{
			return false;
		}
		OutPositions.Reset();
		for (const FArcTQSTargetItem& Item : CompletedQuery.Results)
		{
			const FVector Position = Item.GetLocation(Context.EntityManager);
			if (!Position.ContainsNaN())
			{
				OutPositions.Add(Position);
			}
		}
		State.QueryID = INDEX_NONE;
		State.bCompleted = true;
		State.bSucceeded = CompletedQuery.Status == EArcTQSQueryStatus::Completed
			|| (CompletedQuery.Status == EArcTQSQueryStatus::Failed && CompletedQuery.Results.IsEmpty());
		return true;
	}
}

#if WITH_EDITOR
FName FArcTacticsCollectOwnDataTask::GetIconName() const
{
	return TEXT("AppStyle|Icons.Settings");
}

FColor FArcTacticsCollectOwnDataTask::GetIconColor() const
{
	return FColor(84, 158, 222);
}

FText FArcTacticsCollectOwnDataTask::GetDescription(
	const FGuid&,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner&,
	const bool bRichText) const
{
	const FArcTacticsCollectOwnDataTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsCollectOwnDataTaskInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Transform = Data->bCollectTransform ? NSLOCTEXT("ArcTactics", "Enabled", "on") : NSLOCTEXT("ArcTactics", "Disabled", "off");
	const FText Tags = Data->bCollectOwnedTags ? NSLOCTEXT("ArcTactics", "Enabled", "on") : NSLOCTEXT("ArcTactics", "Disabled", "off");
	const FText Team = Data->bCollectTeam ? NSLOCTEXT("ArcTactics", "Enabled", "on") : NSLOCTEXT("ArcTactics", "Disabled", "off");
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "CollectOwnDataRich", "<b>Self data</> <s>transform {0}, tags {1}, team {2}</>"), Transform, Tags, Team)
		: FText::Format(NSLOCTEXT("ArcTactics", "CollectOwnDataPlain", "Self data: transform {0}, tags {1}, team {2}"), Transform, Tags, Team);
}

FName FArcTacticsCollectTargetsTask::GetIconName() const
{
	return TEXT("AppStyle|Icons.Search");
}

FColor FArcTacticsCollectTargetsTask::GetIconColor() const
{
	return FColor(226, 142, 64);
}

FText FArcTacticsCollectTargetsTask::GetDescription(
	const FGuid&,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner&,
	const bool bRichText) const
{
	const FArcTacticsCollectTargetsTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsCollectTargetsTaskInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Attitude = StaticEnum<EArcTeamAttitude>()->GetDisplayNameTextByValue(static_cast<int64>(Data->TargetAttitude));
	const FText DisplayAttitude = ArcTacticsDataTasksPrivate::Emphasize(Attitude, bRichText);
	const bool bSharedTargetsEnabled = Data->bIncludeSharedTargets
		&& Data->TargetAttitude != EArcTeamAttitude::Friendly
		&& FMath::IsFinite(Data->SharedTargetWindow)
		&& Data->SharedTargetWindow > 0.0f
		&& FMath::IsFinite(Data->SharedTargetGatherRadius)
		&& Data->SharedTargetGatherRadius > 0.0f;
	if (!bSharedTargetsEnabled)
	{
		return bRichText
			? FText::Format(NSLOCTEXT("ArcTactics", "CollectTargetsPerceptionOnlyRich", "{0} targets <s>perception only</>"), DisplayAttitude)
			: FText::Format(NSLOCTEXT("ArcTactics", "CollectTargetsPerceptionOnlyPlain", "{0} targets, perception only"), Attitude);
	}
	const FText Window = FText::AsNumber(Data->SharedTargetWindow);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "CollectTargetsRich", "{0} targets <s>shared window {1}s</>"), DisplayAttitude, Window)
		: FText::Format(NSLOCTEXT("ArcTactics", "CollectTargetsPlain", "{0} targets, shared window {1}s"), Attitude, Window);
}

FName FArcTacticsRunTQSTask::GetIconName() const
{
	return TEXT("AppStyle|Icons.Filter");
}

FColor FArcTacticsRunTQSTask::GetIconColor() const
{
	return FColor(91, 192, 111);
}

FText FArcTacticsRunTQSTask::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsRunTQSTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsRunTQSTaskInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Query = ArcTacticsDataTasksPrivate::Emphasize(
		Data->TargetQuery.IsValid() ? FText::FromString(Data->TargetQuery.GetDefinition()->GetName()) : NSLOCTEXT("ArcTactics", "NoQuery", "No query"),
		bRichText);
	const FText Location = ArcTacticsDataTasksPrivate::GetSourceOrField(
		NodeID,
		GET_MEMBER_NAME_CHECKED(FArcTacticsRunTQSTaskInstanceData, QuerierLocation),
		BindingOwner,
		bRichText);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "RunTQSRich", "Run {0} <s>from</> {1}"), Query, Location)
		: FText::Format(NSLOCTEXT("ArcTactics", "RunTQSPlain", "Run {0} from {1}"), Query, Location);
}

FName FArcTacticsPatrolPointsTask::GetIconName() const
{
	return TEXT("AppStyle|Icons.Refresh");
}

FColor FArcTacticsPatrolPointsTask::GetIconColor() const
{
	return FColor(91, 192, 111);
}

FText FArcTacticsPatrolPointsTask::GetDescription(
	const FGuid&,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner&,
	const bool bRichText) const
{
	const FArcTacticsPatrolPointsTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPatrolPointsTaskInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Query = ArcTacticsDataTasksPrivate::Emphasize(
		Data->TargetQuery.IsValid() ? FText::FromString(Data->TargetQuery.GetDefinition()->GetName()) : NSLOCTEXT("ArcTactics", "NoQuery", "No query"),
		bRichText);
	const FText Range = FText::AsNumber(Data->ArrivalRange);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "PatrolPointsRich", "Patrol {0} <s>hold until within {1} cm</>"), Query, Range)
		: FText::Format(NSLOCTEXT("ArcTactics", "PatrolPointsPlain", "Patrol {0}, hold until within {1} cm"), Query, Range);
}
#endif

const UScriptStruct* FArcTacticsCollectOwnDataTask::GetInstanceDataType() const
{
	return FArcTacticsCollectOwnDataTaskInstanceData::StaticStruct();
}

FText FArcTacticsCollectOwnDataTask::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "CollectOwnDataTask", "Collect Own Data");
}

EArcTacticsTaskStatus FArcTacticsCollectOwnDataTask::Enter(
	FArcTacticsTaskContext& Context,
	FStructView InstanceData) const
{
	FArcTacticsCollectOwnDataTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsCollectOwnDataTaskInstanceData>();
	if (!Data || !Context.EntityManager || !Context.EntityManager->IsEntityActive(Context.Entity))
	{
		return EArcTacticsTaskStatus::Failed;
	}
	Data->Entity = Context.Entity;
	const FMassEntityView EntityView(*Context.EntityManager, Context.Entity);
	if (Data->bCollectTransform)
	{
		const FTransformFragment* Transform = EntityView.GetFragmentDataPtr<FTransformFragment>();
		if (!Transform)
		{
			return EArcTacticsTaskStatus::Failed;
		}
		Data->Transform = Transform->GetTransform();
		Data->Location = Data->Transform.GetLocation();
		Data->Forward = Data->Transform.GetRotation().GetForwardVector();
	}
	if (Data->bCollectOwnedTags)
	{
		const FArcOwnedTagsFragment* OwnedTags = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		if (!OwnedTags)
		{
			return EArcTacticsTaskStatus::Failed;
		}
		Data->OwnedTags = OwnedTags->GetTags().GetTagContainer();
	}
	if (Data->bCollectTeam)
	{
		const FArcMassTeamFragment* Team = EntityView.GetFragmentDataPtr<FArcMassTeamFragment>();
		if (!Team)
		{
			return EArcTacticsTaskStatus::Failed;
		}
		Data->TeamId = Team->TeamId;
	}
	return EArcTacticsTaskStatus::Succeeded;
}

EArcTacticsExecutionRequirement FArcTacticsCollectOwnDataTask::GetRequirements(FConstStructView InstanceData) const
{
	return EArcTacticsExecutionRequirement::Transform
		| EArcTacticsExecutionRequirement::OwnedTags
		| EArcTacticsExecutionRequirement::Team;
}

const UScriptStruct* FArcTacticsCollectTargetsTask::GetInstanceDataType() const
{
	return FArcTacticsCollectTargetsTaskInstanceData::StaticStruct();
}

FText FArcTacticsCollectTargetsTask::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "CollectTargetsTask", "Collect Perception Targets");
}

EArcTacticsTaskStatus FArcTacticsCollectTargetsTask::Enter(
	FArcTacticsTaskContext& Context,
	FStructView InstanceData) const
{
	FArcTacticsCollectTargetsTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsCollectTargetsTaskInstanceData>();
	if (!Data || !Context.World || !Context.EntityManager
		|| !Context.EntityManager->IsEntityActive(Context.Entity))
	{
		return EArcTacticsTaskStatus::Failed;
	}
	Data->Targets.Reset();
	Data->TargetInfos.Reset();
	Data->bHasEligibleTarget = false;
	TArray<double> SeenTimes;
	const FMassEntityView ObserverView(*Context.EntityManager, Context.Entity);
	const FTransformFragment* Transform = ObserverView.GetFragmentDataPtr<FTransformFragment>();
	const FArcMassSightPerceptionResult* Sight = ObserverView.GetFragmentDataPtr<FArcMassSightPerceptionResult>();
	const FArcPerceptionSightSenseConfigFragment* SightConfig =
		Context.EntityManager->GetConstSharedFragmentDataPtr<FArcPerceptionSightSenseConfigFragment>(Context.Entity);
	const FArcMassTeamFragment* Team = ObserverView.GetFragmentDataPtr<FArcMassTeamFragment>();
	if (!Transform || !Sight || !SightConfig)
	{
		return EArcTacticsTaskStatus::Failed;
	}
	const int32 ObserverTeamId = Team ? Team->TeamId : INDEX_NONE;
	const double CurrentTime = Context.World->GetTimeSeconds();
	for (const FArcPerceivedEntity& Perceived : Sight->PerceivedEntities)
	{
		const double Age = FMath::Max(0.0, CurrentTime - Perceived.LastTimeSeen);
		if (Perceived.Entity != Context.Entity
			&& Context.EntityManager->IsEntityActive(Perceived.Entity)
			&& FMath::IsFinite(Perceived.LastTimeSeen)
			&& FMath::IsFinite(SightConfig->ForgetTime)
			&& SightConfig->ForgetTime >= 0.0f
			&& Age <= SightConfig->ForgetTime
			&& ArcTacticsDataTasksPrivate::IsEligibleTarget(
				*Context.EntityManager, ObserverTeamId, Perceived.Entity, *Data))
		{
			ArcTacticsDataTasksPrivate::UpsertTarget(
				Data->TargetInfos, SeenTimes, Perceived.Entity, Perceived.LastKnownLocation, Perceived.LastTimeSeen);
		}
	}

	const double Window = FMath::Min(
		FMath::IsFinite(Data->SharedTargetWindow) ? FMath::Max(0.0, static_cast<double>(Data->SharedTargetWindow)) : 0.0,
		FMath::Max(0.0, ArcSharedTargetKnowledge::GetRetentionSeconds()));
	const float Radius = FMath::IsFinite(Data->SharedTargetGatherRadius)
		? FMath::Max(0.0f, Data->SharedTargetGatherRadius)
		: 0.0f;
	UArcKnowledgeSubsystem* Knowledge = Context.World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (Data->bIncludeSharedTargets && Data->TargetAttitude != EArcTeamAttitude::Friendly
		&& Knowledge && ObserverTeamId != INDEX_NONE && Window > 0.0 && Radius > 0.0f)
	{
		TArray<FArcKnowledgeHandle> Handles;
		Knowledge->QueryKnowledgeInRadius(
			Transform->GetTransform().GetLocation(), Radius, Handles,
			FGameplayTagQuery::MakeQuery_MatchTag(TAG_AI_Knowledge_SharedTarget));
		TSet<FMassEntityHandle> ResolvedTargets;
		for (const FArcKnowledgeHandle& Handle : Handles)
		{
			const FArcKnowledgeEntry* Located = Knowledge->GetKnowledgeEntry(Handle);
			if (!Located || !Located->SourceEntity.IsValid() || Located->SourceEntity == Context.Entity)
			{
				continue;
			}
			bool bAlreadyResolved = false;
			ResolvedTargets.Add(Located->SourceEntity, &bAlreadyResolved);
			if (bAlreadyResolved)
			{
				continue;
			}
			const FArcKnowledgeEntry* Entry = Knowledge->GetKnowledgeEntry(
				ArcSharedTargetKnowledge::FindTeamEntry(*Knowledge, Located->SourceEntity, ObserverTeamId));
			if (!Entry || !FMath::IsFinite(Entry->Timestamp) || Entry->Location.ContainsNaN()
				|| FMath::Max(0.0, CurrentTime - Entry->Timestamp) > Window
				|| !Context.EntityManager->IsEntityActive(Located->SourceEntity)
				|| !ArcTacticsDataTasksPrivate::IsEligibleTarget(
					*Context.EntityManager, ObserverTeamId, Located->SourceEntity, *Data))
			{
				continue;
			}
			ArcTacticsDataTasksPrivate::UpsertTarget(
				Data->TargetInfos, SeenTimes, Located->SourceEntity, Entry->Location, Entry->Timestamp);
		}
	}
	Data->Targets.Reserve(Data->TargetInfos.Num());
	for (const FArcTacticsTarget& Target : Data->TargetInfos)
	{
		Data->Targets.Add(Target.Entity);
	}
	Data->bHasEligibleTarget = !Data->Targets.IsEmpty();
	return EArcTacticsTaskStatus::Succeeded;
}

EArcTacticsExecutionRequirement FArcTacticsCollectTargetsTask::GetRequirements(FConstStructView InstanceData) const
{
	return EArcTacticsExecutionRequirement::Transform
		| EArcTacticsExecutionRequirement::Sight
		| EArcTacticsExecutionRequirement::KnowledgeSubsystem;
}

const UScriptStruct* FArcTacticsRunTQSTask::GetInstanceDataType() const
{
	return FArcTacticsRunTQSTaskInstanceData::StaticStruct();
}

FText FArcTacticsRunTQSTask::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "RunTQSTask", "Run Tactical Query");
}

EArcTacticsTaskStatus FArcTacticsRunTQSTask::Enter(
	FArcTacticsTaskContext& Context,
	FStructView InstanceData) const
{
	FArcTacticsRunTQSTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsRunTQSTaskInstanceData>();
	if (!Data || !Data->TargetQuery.IsValid())
	{
		return EArcTacticsTaskStatus::Failed;
	}
	Data->Positions.Reset();

	FArcTQSQueryContext QueryContext;
	QueryContext.QuerierLocation = Data->QuerierLocation;
	QueryContext.QuerierForward = Data->QuerierForward;
	FArcTQSSelectionOverride SelectionOverride;
	SelectionOverride.SelectionMode = EArcTQSSelectionMode::AllPassing;
	SelectionOverride.MinPassingScore = FMath::IsFinite(Data->MinPassingScore)
		? FMath::Max(0.0f, Data->MinPassingScore)
		: 0.0f;
	return ArcTacticsDataTasksPrivate::SubmitQuery(
		Context, Data->TargetQuery, QueryContext, &SelectionOverride, Data->Query);
}

EArcTacticsTaskStatus FArcTacticsRunTQSTask::Tick(
	FArcTacticsTaskContext&,
	FStructView InstanceData) const
{
	const FArcTacticsRunTQSTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsRunTQSTaskInstanceData>();
	return Data ? ArcTacticsDataTasksPrivate::GetQueryStatus(Data->Query) : EArcTacticsTaskStatus::Failed;
}

void FArcTacticsRunTQSTask::Cancel(FArcTacticsTaskContext& Context, FStructView InstanceData) const
{
	if (FArcTacticsRunTQSTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsRunTQSTaskInstanceData>())
	{
		ArcTacticsDataTasksPrivate::CancelQuery(Context, Data->Query);
	}
}

void FArcTacticsRunTQSTask::OnAsyncQueryCompleted(
	FArcTacticsTaskContext& Context,
	FStructView InstanceData,
	const FArcTacticsAsyncRequestToken& Token,
	FArcTQSQueryInstance& CompletedQuery) const
{
	FArcTacticsRunTQSTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsRunTQSTaskInstanceData>();
	if (!Data || !ArcTacticsDataTasksPrivate::CompleteQuery(Context, Token, CompletedQuery, Data->Query, Data->Positions))
	{
		return;
	}
	Context.QueryReport.bAccepted = true;
	Context.QueryReport.Definition = Data->TargetQuery.GetDefinition();
	Context.QueryReport.AppliedCount = Data->Query.bSucceeded ? Data->Positions.Num() : 0;
}

EArcTacticsExecutionRequirement FArcTacticsRunTQSTask::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::TQSSubsystem;
}

const UScriptStruct* FArcTacticsPatrolPointsTask::GetInstanceDataType() const
{
	return FArcTacticsPatrolPointsTaskInstanceData::StaticStruct();
}

FText FArcTacticsPatrolPointsTask::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "PatrolPointsTask", "Generate Patrol Points");
}

EArcTacticsTaskStatus FArcTacticsPatrolPointsTask::Enter(
	FArcTacticsTaskContext& Context,
	FStructView InstanceData) const
{
	FArcTacticsPatrolPointsTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPatrolPointsTaskInstanceData>();
	if (!Data || !Data->TargetQuery.IsValid() || !Context.EntityManager
		|| !Context.EntityManager->IsEntityActive(Context.Entity))
	{
		return EArcTacticsTaskStatus::Failed;
	}
	Data->Points.Reset();
	FMassEntityView EntityView(*Context.EntityManager, Context.Entity);
	const FTransformFragment* Transform = EntityView.GetFragmentDataPtr<FTransformFragment>();
	FArcMassMemoryFragment* Memory = EntityView.GetSparseFragmentDataPtr<FArcMassMemoryFragment>();
	if (!Memory)
	{
		Context.FailureReason = TEXT("PatrolPointsMissingMemoryFragment");
		return EArcTacticsTaskStatus::Failed;
	}
	if (!Transform)
	{
		return EArcTacticsTaskStatus::Failed;
	}
	const FVector Location = Transform->GetTransform().GetLocation();
	FVector HeldPoint = FVector::ZeroVector;
	if (ArcMassMemory::Read(*Memory, Data->MemoryKey, HeldPoint))
	{
		const float ArrivalRange = FMath::IsFinite(Data->ArrivalRange) ? FMath::Max(0.0f, Data->ArrivalRange) : 0.0f;
		if (!HeldPoint.ContainsNaN() && !ArcPathFollow::IsArrived(Location, HeldPoint, ArrivalRange))
		{
			Data->Points.Add(HeldPoint);
			return EArcTacticsTaskStatus::Succeeded;
		}
		ArcMassMemory::Forget(*Memory, Data->MemoryKey);
	}

	FArcTQSQueryContext QueryContext;
	QueryContext.QuerierLocation = Location;
	QueryContext.QuerierForward = Transform->GetTransform().GetRotation().GetForwardVector();
	return ArcTacticsDataTasksPrivate::SubmitQuery(Context, Data->TargetQuery, QueryContext, nullptr, Data->Query);
}

EArcTacticsTaskStatus FArcTacticsPatrolPointsTask::Tick(
	FArcTacticsTaskContext&,
	FStructView InstanceData) const
{
	const FArcTacticsPatrolPointsTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPatrolPointsTaskInstanceData>();
	return Data ? ArcTacticsDataTasksPrivate::GetQueryStatus(Data->Query) : EArcTacticsTaskStatus::Failed;
}

void FArcTacticsPatrolPointsTask::Cancel(FArcTacticsTaskContext& Context, FStructView InstanceData) const
{
	if (FArcTacticsPatrolPointsTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPatrolPointsTaskInstanceData>())
	{
		ArcTacticsDataTasksPrivate::CancelQuery(Context, Data->Query);
	}
}

void FArcTacticsPatrolPointsTask::OnAsyncQueryCompleted(
	FArcTacticsTaskContext& Context,
	FStructView InstanceData,
	const FArcTacticsAsyncRequestToken& Token,
	FArcTQSQueryInstance& CompletedQuery) const
{
	FArcTacticsPatrolPointsTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPatrolPointsTaskInstanceData>();
	if (!Data || !ArcTacticsDataTasksPrivate::CompleteQuery(Context, Token, CompletedQuery, Data->Query, Data->Points))
	{
		return;
	}
	Context.QueryReport.bAccepted = true;
	Context.QueryReport.Definition = Data->TargetQuery.GetDefinition();
	if (!Data->Query.bSucceeded || Data->Points.IsEmpty() || !Context.EntityManager)
	{
		return;
	}
	Context.QueryReport.AppliedCount = 1;
	Data->Points.SetNum(1);
	FMassEntityView EntityView(*Context.EntityManager, Context.Entity);
	if (FArcMassMemoryFragment* Memory = EntityView.GetSparseFragmentDataPtr<FArcMassMemoryFragment>())
	{
		ArcMassMemory::Write(*Memory, Data->MemoryKey, Data->Points[0]);
	}
}

EArcTacticsExecutionRequirement FArcTacticsPatrolPointsTask::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::Transform
		| EArcTacticsExecutionRequirement::TQSSubsystem;
}
