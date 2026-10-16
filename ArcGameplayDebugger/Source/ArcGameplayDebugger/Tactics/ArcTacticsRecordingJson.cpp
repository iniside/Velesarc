// Copyright Lukasz Baran. All Rights Reserved.

#include "Tactics/ArcTacticsRecordingJson.h"

#include "Debugger/ArcTacticsDebug.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Tactics/ArcTacticsRecording.h"

namespace ArcTacticsRecordingJsonPrivate
{
	using FJsonValues = TArray<TSharedPtr<FJsonValue>>;

	struct FCandidateStats
	{
		int32 Evaluated = 0;
		int32 Selected = 0;
		TMap<FString, TMap<FString, int32>> Rejections;
	};

	struct FActionStats
	{
		int32 Spans = 0;
		TMap<FString, int32> EndStatus;
	};

	template <typename TEnum>
	FString EnumName(const TEnum Value)
	{
		const FString Name = StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
		return Name.IsEmpty() ? FString::FromInt(static_cast<int32>(Value)) : Name;
	}

	const TCHAR* EnumName(const EArcTacticsQuerySource Value)
	{
		switch (Value)
		{
		case EArcTacticsQuerySource::GlobalTask:
			return TEXT("GlobalTask");
		case EArcTacticsQuerySource::Step:
			return TEXT("Step");
		}
		return TEXT("Unknown");
	}

	const TCHAR* EnumName(const EArcTacticsRecordedActionEventType Value)
	{
		switch (Value)
		{
		case EArcTacticsRecordedActionEventType::Entered:
			return TEXT("Entered");
		case EArcTacticsRecordedActionEventType::StepChanged:
			return TEXT("StepChanged");
		case EArcTacticsRecordedActionEventType::Path:
			return TEXT("Path");
		case EArcTacticsRecordedActionEventType::Query:
			return TEXT("Query");
		case EArcTacticsRecordedActionEventType::Ended:
			return TEXT("Ended");
		}
		return TEXT("Unknown");
	}

	const TCHAR* QueryItemsName(const EArcTacticsRecordingQueryItems Value)
	{
		switch (Value)
		{
		case EArcTacticsRecordingQueryItems::All:
			return TEXT("all");
		case EArcTacticsRecordingQueryItems::Valid:
			return TEXT("valid");
		case EArcTacticsRecordingQueryItems::None:
			return TEXT("none");
		}
		return TEXT("all");
	}

	TSharedPtr<FJsonValue> Null()
	{
		return MakeShared<FJsonValueNull>();
	}

	TSharedPtr<FJsonValue> StringValue(const FString& Value)
	{
		return MakeShared<FJsonValueString>(Value);
	}

	TSharedPtr<FJsonValue> Number(const double Value)
	{
		if (!FMath::IsFinite(Value))
		{
			return Null();
		}
		return MakeShared<FJsonValueNumberString>(FString::SanitizeFloat(Value, 0));
	}

	TSharedPtr<FJsonValue> Integer(const int64 Value)
	{
		return MakeShared<FJsonValueNumber>(static_cast<double>(Value));
	}

	TSharedPtr<FJsonValue> OptionalIndex(const int32 Index)
	{
		return Index == INDEX_NONE ? Null() : Integer(Index);
	}

	TSharedPtr<FJsonValue> Vector(const FVector3f& Value)
	{
		FJsonValues Components;
		Components.Reserve(3);
		Components.Add(Number(Value.X));
		Components.Add(Number(Value.Y));
		Components.Add(Number(Value.Z));
		return MakeShared<FJsonValueArray>(MoveTemp(Components));
	}

	TSharedPtr<FJsonValue> Position(const FArcTacticsRecordedPosition& Value)
	{
		return Value.bValid ? Vector(Value.Location) : Null();
	}

	TSharedPtr<FJsonValue> IndexArray(const TArray<int32>& Indices)
	{
		FJsonValues Values;
		Values.Reserve(Indices.Num());
		for (const int32 Index : Indices)
		{
			Values.Add(Integer(Index));
		}
		return MakeShared<FJsonValueArray>(MoveTemp(Values));
	}

	TSharedPtr<FJsonValue> StringArray(const TArray<FString>& Strings)
	{
		FJsonValues Values;
		Values.Reserve(Strings.Num());
		for (const FString& Value : Strings)
		{
			Values.Add(StringValue(Value));
		}
		return MakeShared<FJsonValueArray>(MoveTemp(Values));
	}

	TSharedPtr<FJsonValue> NumberArray(const TArray<float>& Numbers)
	{
		FJsonValues Values;
		Values.Reserve(Numbers.Num());
		for (const float Value : Numbers)
		{
			Values.Add(Number(Value));
		}
		return MakeShared<FJsonValueArray>(MoveTemp(Values));
	}

	FString WorldIdString(const uint64 WorldId)
	{
		return FString::Printf(TEXT("%llu"), WorldId);
	}

	bool MatchesEntity(const FArcTacticsRecordedEntity& Entity, const FArcTacticsRecordingJsonOptions& Options)
	{
		return (!Options.EntityIndex.IsSet() || Entity.EntityIndex == Options.EntityIndex.GetValue())
			&& (!Options.EntitySerial.IsSet() || Entity.EntitySerial == Options.EntitySerial.GetValue());
	}

	bool InTimeRange(const double Time, const FArcTacticsRecordingJsonOptions& Options)
	{
		return (!Options.FromTime.IsSet() || Time >= Options.FromTime.GetValue())
			&& (!Options.ToTime.IsSet() || Time <= Options.ToTime.GetValue());
	}

	bool OverlapsTimeRange(const double StartTime, const double EndTime, const FArcTacticsRecordingJsonOptions& Options)
	{
		return (!Options.FromTime.IsSet() || EndTime >= Options.FromTime.GetValue())
			&& (!Options.ToTime.IsSet() || StartTime <= Options.ToTime.GetValue());
	}

	TSharedPtr<FJsonObject> WriteTarget(const FArcTacticsRecordedTarget& Target)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("type"), EnumName(Target.Type));
		if (Target.Type == EArcTacticsValueIdentity::Entity)
		{
			Object->SetField(TEXT("entityIndex"), Integer(Target.EntityIndex));
			Object->SetField(TEXT("entitySerial"), Integer(Target.EntitySerial));
		}
		Object->SetField(TEXT("location"), Target.bHasLocation ? Vector(Target.Location) : Null());
		return Object;
	}

	const TCHAR* ConsiderationState(const FArcTacticsRecordedCandidate& Candidate, const int32 Index, const int32 EvaluatedCount)
	{
		if (Index == Candidate.VetoIndex)
		{
			return TEXT("veto");
		}
		if (Index >= EvaluatedCount)
		{
			return TEXT("notEvaluated");
		}
		if (Index == Candidate.EarlyOutIndex)
		{
			return TEXT("earlyOut");
		}
		return TEXT("scored");
	}

	TSharedPtr<FJsonObject> WriteCandidate(const FArcTacticsRecordedCandidate& Candidate, const bool bSelected)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetField(TEXT("order"), OptionalIndex(Candidate.Order));
		Object->SetStringField(TEXT("package"), Candidate.Package);
		Object->SetStringField(TEXT("name"), Candidate.Name);
		Object->SetField(TEXT("sourceIndex"), OptionalIndex(Candidate.SourceIndex));
		Object->SetBoolField(TEXT("fromSelector"), Candidate.bFromSelector);
		Object->SetObjectField(TEXT("target"), WriteTarget(Candidate.Target));
		Object->SetField(TEXT("score"), Number(Candidate.Score));
		Object->SetField(TEXT("priority"), Candidate.bHasPriority ? Integer(Candidate.Priority) : Null());
		Object->SetStringField(TEXT("outcome"), EnumName(Candidate.Outcome));
		Object->SetStringField(TEXT("reason"), Candidate.Reason);
		Object->SetBoolField(TEXT("selected"), bSelected);

		const int32 EvaluatedCount = FMath::Min(Candidate.Raw.Num(), Candidate.Curved.Num());
		FJsonValues Considerations;
		Considerations.Reserve(Candidate.ConsiderationNames.Num());
		for (int32 Index = 0; Index < Candidate.ConsiderationNames.Num(); ++Index)
		{
			const bool bEvaluated = Index < EvaluatedCount;
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Candidate.ConsiderationNames[Index]);
			Row->SetField(TEXT("raw"), bEvaluated ? Number(Candidate.Raw[Index]) : Null());
			Row->SetField(TEXT("curved"), bEvaluated ? Number(Candidate.Curved[Index]) : Null());
			Row->SetStringField(TEXT("state"), ConsiderationState(Candidate, Index, EvaluatedCount));
			Considerations.Add(MakeShared<FJsonValueObject>(Row));
		}
		Object->SetArrayField(TEXT("considerations"), MoveTemp(Considerations));
		return Object;
	}

	TSharedPtr<FJsonObject> WriteDecision(const FArcTacticsRecordedDecision& Decision, const int32 DecisionIndex, TMap<FString, FCandidateStats>& CandidateStats)
	{
		const FArcTacticsRecordedCandidate* Selected = Decision.FindSelectedCandidate();
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetField(TEXT("index"), Integer(DecisionIndex));
		Object->SetField(TEXT("time"), Number(Decision.Time));
		Object->SetField(TEXT("generation"), Integer(Decision.DecisionGeneration));
		Object->SetStringField(TEXT("selectionMode"), EnumName(Decision.SelectionMode));
		FJsonValues Packages;
		Packages.Reserve(Decision.Packages.Num());
		for (const FArcTacticsRecordedDecisionPackage& Package : Decision.Packages)
		{
			TSharedPtr<FJsonObject> PackageObject = MakeShared<FJsonObject>();
			PackageObject->SetStringField(TEXT("config"), Package.ConfigName);
			PackageObject->SetField(TEXT("minScore"), Number(Package.MinScore));
			PackageObject->SetBoolField(TEXT("active"), Package.bActive);
			PackageObject->SetBoolField(TEXT("excluded"), Package.bExcluded);
			PackageObject->SetStringField(TEXT("diagnostic"), Package.Diagnostic);
			Packages.Add(MakeShared<FJsonValueObject>(PackageObject));
		}
		Object->SetArrayField(TEXT("packages"), MoveTemp(Packages));
		Object->SetField(TEXT("selectedOrder"), OptionalIndex(Decision.SelectedOrder));
		Object->SetField(TEXT("selected"), Selected ? StringValue(Selected->Name) : Null());
		Object->SetStringField(TEXT("status"), EnumName(Decision.Status));
		Object->SetStringField(TEXT("diagnostic"), Decision.Diagnostic);
		Object->SetField(TEXT("position"), Position(Decision.Position));
		Object->SetField(TEXT("queries"), IndexArray(Decision.QueryIndices));

		FJsonValues Candidates;
		Candidates.Reserve(Decision.Candidates.Num());
		for (const FArcTacticsRecordedCandidate& Candidate : Decision.Candidates)
		{
			const bool bSelected = &Candidate == Selected;
			Candidates.Add(MakeShared<FJsonValueObject>(WriteCandidate(Candidate, bSelected)));

			FCandidateStats& Stats = CandidateStats.FindOrAdd(
				Candidate.Package.IsEmpty() ? Candidate.Name : Candidate.Package + TEXT("/") + Candidate.Name);
			++Stats.Evaluated;
			if (bSelected)
			{
				++Stats.Selected;
			}
			if (Candidate.Outcome != EArcTacticsCandidateOutcome::Accepted)
			{
				const FString Reason = Candidate.Reason.IsEmpty() ? FString(TEXT("none")) : Candidate.Reason;
				++Stats.Rejections.FindOrAdd(EnumName(Candidate.Outcome)).FindOrAdd(Reason);
			}
		}
		Object->SetArrayField(TEXT("candidates"), MoveTemp(Candidates));
		return Object;
	}

	TSharedPtr<FJsonObject> WriteActionSpan(const FArcTacticsRecordedActionSpan& Span, const int32 SpanIndex, TMap<FString, FActionStats>& ActionStats)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetField(TEXT("index"), Integer(SpanIndex));
		Object->SetStringField(TEXT("package"), Span.Package);
		Object->SetStringField(TEXT("name"), Span.Name);
		Object->SetField(TEXT("actionSerial"), Integer(Span.ActionSerial));
		Object->SetField(TEXT("sourceIndex"), OptionalIndex(Span.SourceIndex));
		Object->SetBoolField(TEXT("fromSelector"), Span.bFromSelector);
		Object->SetObjectField(TEXT("target"), WriteTarget(Span.Target));
		Object->SetField(TEXT("startTime"), Number(Span.StartTime));
		Object->SetField(TEXT("endTime"), Number(Span.EndTime));
		Object->SetBoolField(TEXT("enteredInRecording"), Span.bEnteredInRecording);
		Object->SetBoolField(TEXT("ended"), Span.bEnded);
		Object->SetBoolField(TEXT("openAtRecordingEnd"), Span.bOpenAtRecordingEnd);
		Object->SetField(TEXT("startPosition"), Position(Span.StartPosition));
		Object->SetField(TEXT("endPosition"), Position(Span.EndPosition));
		if (Span.bEnded)
		{
			TSharedPtr<FJsonObject> End = MakeShared<FJsonObject>();
			End->SetStringField(TEXT("status"), ArcTacticsDebug::EndStatusText(Span.End.Status));
			End->SetStringField(TEXT("reason"), ArcTacticsDebug::EndReasonText(Span.End.Reason));
			End->SetBoolField(TEXT("cooldownStamped"), Span.End.bCooldownStamped);
			End->SetField(TEXT("cooldown"), Number(Span.End.Cooldown));
			End->SetStringField(TEXT("historyTag"), Span.End.HistoryTag);
			End->SetBoolField(TEXT("historyAppended"), Span.End.bHistoryAppended);
			Object->SetObjectField(TEXT("end"), End);
		}
		else
		{
			Object->SetField(TEXT("end"), Null());
		}
		Object->SetField(TEXT("paths"), IndexArray(Span.PathIndices));
		Object->SetField(TEXT("queries"), IndexArray(Span.QueryIndices));

		FActionStats& Stats = ActionStats.FindOrAdd(!Span.bEnteredInRecording
			? FString(TEXT("<enteredBeforeRecording>"))
			: Span.Package.IsEmpty() ? Span.Name : Span.Package + TEXT("/") + Span.Name);
		++Stats.Spans;
		++Stats.EndStatus.FindOrAdd(Span.bEnded ? FString(ArcTacticsDebug::EndStatusText(Span.End.Status)) : FString(TEXT("NotEnded")));
		return Object;
	}

	TSharedPtr<FJsonObject> WriteEvent(const FArcTacticsRecordedActionEvent& Event, const int32 EventIndex)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetField(TEXT("index"), Integer(EventIndex));
		Object->SetStringField(TEXT("type"), EnumName(Event.Type));
		Object->SetField(TEXT("time"), Number(Event.Time));
		Object->SetField(TEXT("span"), OptionalIndex(Event.SpanIndex));
		Object->SetField(TEXT("position"), Position(Event.Position));
		switch (Event.Type)
		{
		case EArcTacticsRecordedActionEventType::StepChanged:
			Object->SetField(TEXT("stepIndex"), OptionalIndex(Event.StepIndex));
			Object->SetField(TEXT("stepCount"), Integer(Event.StepCount));
			Object->SetStringField(TEXT("stepName"), Event.StepName);
			Object->SetStringField(TEXT("stepType"), Event.StepTypeName);
			break;
		case EArcTacticsRecordedActionEventType::Path:
			Object->SetField(TEXT("path"), OptionalIndex(Event.PathIndex));
			break;
		case EArcTacticsRecordedActionEventType::Query:
			Object->SetField(TEXT("query"), OptionalIndex(Event.QueryIndex));
			break;
		case EArcTacticsRecordedActionEventType::Entered:
		case EArcTacticsRecordedActionEventType::Ended:
			break;
		}
		return Object;
	}

	TSharedPtr<FJsonObject> WritePath(const FArcTacticsRecordedPath& Path, const int32 PathIndex)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetField(TEXT("index"), Integer(PathIndex));
		Object->SetField(TEXT("time"), Number(Path.Time));
		Object->SetField(TEXT("span"), OptionalIndex(Path.SpanIndex));
		Object->SetBoolField(TEXT("repath"), Path.bRepath);
		Object->SetBoolField(TEXT("succeeded"), Path.bSucceeded);
		Object->SetBoolField(TEXT("partial"), Path.bPartial);
		Object->SetField(TEXT("start"), Vector(Path.Start));
		Object->SetField(TEXT("end"), Vector(Path.End));
		FJsonValues Points;
		Points.Reserve(Path.Points.Num());
		for (const FVector3f& Point : Path.Points)
		{
			Points.Add(Vector(Point));
		}
		Object->SetArrayField(TEXT("points"), MoveTemp(Points));
		return Object;
	}

	void SetItemEntity(const TSharedPtr<FJsonObject>& Object, const FArcTacticsRecordedQueryItem& Item)
	{
		if (Item.EntityIndex != 0 || Item.EntitySerial != 0)
		{
			Object->SetField(TEXT("entityIndex"), Integer(Item.EntityIndex));
			Object->SetField(TEXT("entitySerial"), Integer(Item.EntitySerial));
		}
	}

	TSharedPtr<FJsonObject> WriteQueryItem(const FArcTacticsRecordedQuery& Query, const int32 ItemIndex)
	{
		const FArcTacticsRecordedQueryItem& Item = Query.Items[ItemIndex];
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetField(TEXT("index"), Integer(ItemIndex));
		Object->SetField(TEXT("location"), Vector(Item.Location));
		Object->SetField(TEXT("score"), Number(Item.Score));
		Object->SetBoolField(TEXT("valid"), Item.bValid);
		Object->SetBoolField(TEXT("selected"), Item.bSelected);
		Object->SetBoolField(TEXT("applied"), Item.bApplied);
		SetItemEntity(Object, Item);
		if (!Query.Steps.IsEmpty())
		{
			FJsonValues StepRaw;
			FJsonValues StepCumulative;
			StepRaw.Reserve(Query.Steps.Num());
			StepCumulative.Reserve(Query.Steps.Num());
			for (const FArcTacticsRecordedQueryStep& Step : Query.Steps)
			{
				StepRaw.Add(Step.RawScores.IsValidIndex(ItemIndex) ? Number(Step.RawScores[ItemIndex]) : Null());
				StepCumulative.Add(Step.CumulativeScores.IsValidIndex(ItemIndex) ? Number(Step.CumulativeScores[ItemIndex]) : Null());
			}
			Object->SetArrayField(TEXT("stepRaw"), MoveTemp(StepRaw));
			Object->SetArrayField(TEXT("stepCumulative"), MoveTemp(StepCumulative));
		}
		return Object;
	}

	TSharedPtr<FJsonObject> WriteQuery(const FArcTacticsRecordedQuery& Query, const int32 QueryIndex, const EArcTacticsRecordingQueryItems QueryItems)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetField(TEXT("index"), Integer(QueryIndex));
		Object->SetField(TEXT("time"), Number(Query.Time));
		Object->SetField(TEXT("generation"), Integer(Query.DecisionGeneration));
		Object->SetStringField(TEXT("sourceKind"), EnumName(Query.SourceKind));
		Object->SetStringField(TEXT("sourceName"), Query.SourceName);
		Object->SetField(TEXT("actionSerial"), Integer(Query.ActionSerial));
		Object->SetStringField(TEXT("queryName"), Query.QueryName);
		Object->SetField(TEXT("queryId"), OptionalIndex(Query.QueryId));
		Object->SetStringField(TEXT("status"), EnumName(Query.Status));
		Object->SetStringField(TEXT("selectionMode"), EnumName(Query.SelectionMode));
		Object->SetField(TEXT("querier"), Position(Query.QuerierLocation));
		Object->SetBoolField(TEXT("complete"), Query.bComplete);
		Object->SetField(TEXT("decision"), OptionalIndex(Query.DecisionIndex));
		Object->SetField(TEXT("span"), OptionalIndex(Query.SpanIndex));

		int32 ValidCount = 0;
		int32 SelectedCount = 0;
		int32 WinnerIndex = INDEX_NONE;
		for (int32 ItemIndex = 0; ItemIndex < Query.Items.Num(); ++ItemIndex)
		{
			const FArcTacticsRecordedQueryItem& Item = Query.Items[ItemIndex];
			ValidCount += Item.bValid ? 1 : 0;
			if (Item.bSelected)
			{
				++SelectedCount;
				if (WinnerIndex == INDEX_NONE)
				{
					WinnerIndex = ItemIndex;
				}
			}
		}
		Object->SetField(TEXT("totalItemCount"), Integer(Query.TotalItemCount));
		Object->SetField(TEXT("recordedItemCount"), Integer(Query.Items.Num()));
		Object->SetField(TEXT("validItemCount"), Integer(ValidCount));
		Object->SetField(TEXT("selectedItemCount"), Integer(SelectedCount));
		if (WinnerIndex != INDEX_NONE)
		{
			const FArcTacticsRecordedQueryItem& Winner = Query.Items[WinnerIndex];
			TSharedPtr<FJsonObject> WinnerObject = MakeShared<FJsonObject>();
			WinnerObject->SetField(TEXT("index"), Integer(WinnerIndex));
			WinnerObject->SetField(TEXT("location"), Vector(Winner.Location));
			WinnerObject->SetField(TEXT("score"), Number(Winner.Score));
			SetItemEntity(WinnerObject, Winner);
			Object->SetObjectField(TEXT("winner"), WinnerObject);
		}
		else
		{
			Object->SetField(TEXT("winner"), Null());
		}

		FJsonValues Steps;
		Steps.Reserve(Query.Steps.Num());
		for (const FArcTacticsRecordedQueryStep& Step : Query.Steps)
		{
			TSharedPtr<FJsonObject> StepObject = MakeShared<FJsonObject>();
			StepObject->SetStringField(TEXT("name"), Step.Name);
			StepObject->SetStringField(TEXT("type"), EnumName(Step.Type));
			StepObject->SetField(TEXT("filteredCount"), Integer(Step.FilteredCount));
			Steps.Add(MakeShared<FJsonValueObject>(StepObject));
		}
		Object->SetArrayField(TEXT("steps"), MoveTemp(Steps));

		if (QueryItems != EArcTacticsRecordingQueryItems::None)
		{
			FJsonValues Items;
			Items.Reserve(QueryItems == EArcTacticsRecordingQueryItems::Valid ? ValidCount : Query.Items.Num());
			for (int32 ItemIndex = 0; ItemIndex < Query.Items.Num(); ++ItemIndex)
			{
				if (QueryItems == EArcTacticsRecordingQueryItems::Valid && !Query.Items[ItemIndex].bValid)
				{
					continue;
				}
				Items.Add(MakeShared<FJsonValueObject>(WriteQueryItem(Query, ItemIndex)));
			}
			Object->SetArrayField(TEXT("items"), MoveTemp(Items));
		}
		return Object;
	}

	TSharedPtr<FJsonObject> WriteEntitySummary(const FArcTacticsRecordedEntity& Entity, const int32 DecisionCount,
		const TMap<FString, FCandidateStats>& CandidateStats, const TMap<FString, FActionStats>& ActionStats)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("worldId"), WorldIdString(Entity.WorldId));
		Object->SetField(TEXT("entityIndex"), Integer(Entity.EntityIndex));
		Object->SetField(TEXT("entitySerial"), Integer(Entity.EntitySerial));
		Object->SetField(TEXT("configs"), StringArray(Entity.ConfigNames));
		Object->SetField(TEXT("decisionCount"), Integer(DecisionCount));

		TSharedPtr<FJsonObject> Candidates = MakeShared<FJsonObject>();
		for (const TPair<FString, FCandidateStats>& Pair : CandidateStats)
		{
			TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
			Stats->SetField(TEXT("evaluated"), Integer(Pair.Value.Evaluated));
			Stats->SetField(TEXT("selected"), Integer(Pair.Value.Selected));
			TSharedPtr<FJsonObject> Rejections = MakeShared<FJsonObject>();
			for (const TPair<FString, TMap<FString, int32>>& Outcome : Pair.Value.Rejections)
			{
				TSharedPtr<FJsonObject> Reasons = MakeShared<FJsonObject>();
				for (const TPair<FString, int32>& Reason : Outcome.Value)
				{
					Reasons->SetField(Reason.Key, Integer(Reason.Value));
				}
				Rejections->SetObjectField(Outcome.Key, Reasons);
			}
			Stats->SetObjectField(TEXT("rejections"), Rejections);
			Candidates->SetObjectField(Pair.Key, Stats);
		}
		Object->SetObjectField(TEXT("candidates"), Candidates);

		TSharedPtr<FJsonObject> Actions = MakeShared<FJsonObject>();
		for (const TPair<FString, FActionStats>& Pair : ActionStats)
		{
			TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
			Stats->SetField(TEXT("spans"), Integer(Pair.Value.Spans));
			TSharedPtr<FJsonObject> EndStatus = MakeShared<FJsonObject>();
			for (const TPair<FString, int32>& Status : Pair.Value.EndStatus)
			{
				EndStatus->SetField(Status.Key, Integer(Status.Value));
			}
			Stats->SetObjectField(TEXT("endStatus"), EndStatus);
			Actions->SetObjectField(Pair.Key, Stats);
		}
		Object->SetObjectField(TEXT("actions"), Actions);
		return Object;
	}

	TSharedPtr<FJsonObject> WriteFilter(const FArcTacticsRecordingJsonOptions& Options)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		if (Options.EntityIndex.IsSet())
		{
			TSharedPtr<FJsonObject> Entity = MakeShared<FJsonObject>();
			Entity->SetField(TEXT("index"), Integer(Options.EntityIndex.GetValue()));
			if (Options.EntitySerial.IsSet())
			{
				Entity->SetField(TEXT("serial"), Integer(Options.EntitySerial.GetValue()));
			}
			Object->SetObjectField(TEXT("entity"), Entity);
		}
		if (Options.FromTime.IsSet())
		{
			Object->SetField(TEXT("fromTime"), Number(Options.FromTime.GetValue()));
		}
		if (Options.ToTime.IsSet())
		{
			Object->SetField(TEXT("toTime"), Number(Options.ToTime.GetValue()));
		}
		Object->SetStringField(TEXT("queryItems"), QueryItemsName(Options.QueryItems));
		return Object;
	}
}

FArcTacticsRecordingJsonResult ArcTacticsRecordingJson::Write(const FArcTacticsRecording& Recording, const FArcTacticsRecordingJsonOptions& Options)
{
	using namespace ArcTacticsRecordingJsonPrivate;

	FJsonValues Worlds;
	Worlds.Reserve(Recording.Worlds.Num());
	for (const FArcTacticsRecordedWorld& World : Recording.Worlds)
	{
		TSharedPtr<FJsonObject> WorldObject = MakeShared<FJsonObject>();
		WorldObject->SetStringField(TEXT("worldId"), WorldIdString(World.WorldId));
		WorldObject->SetField(TEXT("originWorldTime"), Number(World.OriginWorldTime));
		WorldObject->SetBoolField(TEXT("hasRecordingStarted"), World.bHasRecordingStarted);
		Worlds.Add(MakeShared<FJsonValueObject>(WorldObject));
	}

	FJsonValues Entities;
	FJsonValues EntitySummaries;
	for (const FArcTacticsRecordedEntity& Entity : Recording.Entities)
	{
		if (!MatchesEntity(Entity, Options))
		{
			continue;
		}

		TMap<FString, FCandidateStats> CandidateStats;
		TMap<FString, FActionStats> ActionStats;

		FJsonValues Decisions;
		for (int32 Index = 0; Index < Entity.Decisions.Num(); ++Index)
		{
			if (InTimeRange(Entity.Decisions[Index].Time, Options))
			{
				Decisions.Add(MakeShared<FJsonValueObject>(WriteDecision(Entity.Decisions[Index], Index, CandidateStats)));
			}
		}

		FJsonValues Actions;
		for (int32 Index = 0; Index < Entity.Actions.Num(); ++Index)
		{
			const FArcTacticsRecordedActionSpan& Span = Entity.Actions[Index];
			if (OverlapsTimeRange(Span.StartTime, Span.EndTime, Options))
			{
				Actions.Add(MakeShared<FJsonValueObject>(WriteActionSpan(Span, Index, ActionStats)));
			}
		}

		FJsonValues Events;
		for (int32 Index = 0; Index < Entity.Events.Num(); ++Index)
		{
			const FArcTacticsRecordedActionEvent& Event = Entity.Events[Index];
			if (InTimeRange(Event.Time, Options))
			{
				Events.Add(MakeShared<FJsonValueObject>(WriteEvent(Event, Index)));
			}
		}

		FJsonValues Paths;
		for (int32 Index = 0; Index < Entity.Paths.Num(); ++Index)
		{
			if (InTimeRange(Entity.Paths[Index].Time, Options))
			{
				Paths.Add(MakeShared<FJsonValueObject>(WritePath(Entity.Paths[Index], Index)));
			}
		}

		FJsonValues Queries;
		for (int32 Index = 0; Index < Entity.Queries.Num(); ++Index)
		{
			if (InTimeRange(Entity.Queries[Index].Time, Options))
			{
				Queries.Add(MakeShared<FJsonValueObject>(WriteQuery(Entity.Queries[Index], Index, Options.QueryItems)));
			}
		}

		const int32 DecisionCount = Decisions.Num();
		TSharedPtr<FJsonObject> EntityObject = MakeShared<FJsonObject>();
		EntityObject->SetStringField(TEXT("worldId"), WorldIdString(Entity.WorldId));
		EntityObject->SetField(TEXT("entityIndex"), Integer(Entity.EntityIndex));
		EntityObject->SetField(TEXT("entitySerial"), Integer(Entity.EntitySerial));
		EntityObject->SetField(TEXT("configs"), StringArray(Entity.ConfigNames));
		EntityObject->SetArrayField(TEXT("decisions"), MoveTemp(Decisions));
		EntityObject->SetArrayField(TEXT("actions"), MoveTemp(Actions));
		EntityObject->SetArrayField(TEXT("events"), MoveTemp(Events));
		EntityObject->SetArrayField(TEXT("paths"), MoveTemp(Paths));
		EntityObject->SetArrayField(TEXT("queries"), MoveTemp(Queries));
		Entities.Add(MakeShared<FJsonValueObject>(EntityObject));

		EntitySummaries.Add(MakeShared<FJsonValueObject>(WriteEntitySummary(Entity, DecisionCount, CandidateStats, ActionStats)));
	}

	FArcTacticsRecordingJsonResult Result;
	Result.Summary = MakeShared<FJsonObject>();
	Result.Summary->SetStringField(TEXT("file"), Recording.FilePath);
	Result.Summary->SetField(TEXT("duration"), Number(Recording.Duration));
	Result.Summary->SetField(TEXT("entityCount"), Integer(EntitySummaries.Num()));
	Result.Summary->SetField(TEXT("unclaimedCandidateCount"), Integer(Recording.UnclaimedCandidateCount));
	Result.Summary->SetField(TEXT("unclaimedQueryCount"), Integer(Recording.UnclaimedQueryCount));
	Result.Summary->SetObjectField(TEXT("filter"), WriteFilter(Options));
	Result.Summary->SetArrayField(TEXT("entities"), MoveTemp(EntitySummaries));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("file"), Recording.FilePath);
	Root->SetField(TEXT("duration"), Number(Recording.Duration));
	Root->SetArrayField(TEXT("worlds"), MoveTemp(Worlds));
	Root->SetObjectField(TEXT("summary"), Result.Summary);
	Root->SetArrayField(TEXT("entities"), MoveTemp(Entities));

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result.Json);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Result;
}
