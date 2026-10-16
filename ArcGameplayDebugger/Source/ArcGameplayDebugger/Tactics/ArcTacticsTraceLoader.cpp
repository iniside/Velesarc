// Copyright Lukasz Baran. All Rights Reserved.

#include "Tactics/ArcTacticsTraceLoader.h"

#include "Tactics/ArcTacticsRecording.h"

#if WITH_ARC_TACTICS_TRACE_ANALYSIS

#include "Trace/Analysis.h"
#include "Trace/Analyzer.h"
#include "Trace/DataStream.h"

namespace ArcTacticsTraceLoaderPrivate
{
	enum ERoute : uint16
	{
		RouteId_RecordingStarted,
		RouteId_Candidate,
		RouteId_DecisionPackage,
		RouteId_Decision,
		RouteId_ActionEntered,
		RouteId_ActionStepChanged,
		RouteId_ActionEnded,
		RouteId_PathRequested,
		RouteId_TQSQuery
	};

	struct FEntityKey
	{
		uint64 WorldId = 0;
		uint32 EntityIndex = 0;
		uint32 EntitySerial = 0;

		bool operator==(const FEntityKey& Other) const
		{
			return WorldId == Other.WorldId && EntityIndex == Other.EntityIndex && EntitySerial == Other.EntitySerial;
		}

		friend uint32 GetTypeHash(const FEntityKey& Key)
		{
			return HashCombineFast(GetTypeHash(Key.WorldId), HashCombineFast(GetTypeHash(Key.EntityIndex), GetTypeHash(Key.EntitySerial)));
		}
	};

	struct FQueryAssembly
	{
		FArcTacticsRecordedQuery Query;
		int32 NextChunkIndex = 0;
		int32 ChunkCount = 0;
	};

	struct FEntityState
	{
		TArray<FArcTacticsRecordedCandidate> PendingCandidates;
		TArray<FArcTacticsRecordedDecisionPackage> PendingPackages;
		TArray<int32> PendingQueryIndices;
		TMap<int32, FQueryAssembly> AssemblingQueries;
		int32 OpenSpanIndex = INDEX_NONE;
	};

	FString ReadString(const UE::Trace::IAnalyzer::FEventData& EventData, const ANSICHAR* FieldName)
	{
		FString Value;
		EventData.GetString(FieldName, Value);
		return Value;
	}

	TArray<float> ReadFloatArray(const UE::Trace::IAnalyzer::FEventData& EventData, const ANSICHAR* FieldName)
	{
		const TArrayView<const float> View = EventData.GetArrayView<float>(FieldName);
		return TArray<float>(View.GetData(), View.Num());
	}

	FArcTacticsRecordedTarget ReadTarget(const UE::Trace::IAnalyzer::FEventData& EventData)
	{
		FArcTacticsRecordedTarget Target;
		Target.Type = static_cast<EArcTacticsValueIdentity>(EventData.GetValue<uint8>("IdentityType"));
		Target.EntityIndex = EventData.GetValue<uint32>("TargetIndex");
		Target.EntitySerial = EventData.GetValue<uint32>("TargetSerial");
		Target.Location = FVector3f(
			EventData.GetValue<float>("TargetX"),
			EventData.GetValue<float>("TargetY"),
			EventData.GetValue<float>("TargetZ"));
		Target.bHasLocation = EventData.GetValue<bool>("bHasTargetLocation");
		return Target;
	}

	FArcTacticsRecordedPosition ReadPosition(const UE::Trace::IAnalyzer::FEventData& EventData)
	{
		FArcTacticsRecordedPosition Position;
		Position.bValid = EventData.GetValue<bool>("bHasLocation");
		Position.Location = FVector3f(
			EventData.GetValue<float>("LocationX"),
			EventData.GetValue<float>("LocationY"),
			EventData.GetValue<float>("LocationZ"));
		return Position;
	}

	FVector3f ReadVector(const UE::Trace::IAnalyzer::FEventData& EventData, const ANSICHAR* FieldX, const ANSICHAR* FieldY, const ANSICHAR* FieldZ)
	{
		return FVector3f(
			EventData.GetValue<float>(FieldX),
			EventData.GetValue<float>(FieldY),
			EventData.GetValue<float>(FieldZ));
	}

	class FAnalyzer : public UE::Trace::IAnalyzer
	{
	public:
		explicit FAnalyzer(FArcTacticsRecording& InRecording)
			: Recording(InRecording)
		{
		}

		virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override
		{
			FInterfaceBuilder& Builder = Context.InterfaceBuilder;
			Builder.RouteEvent(RouteId_RecordingStarted, "ArcTactics", "RecordingStarted");
			Builder.RouteEvent(RouteId_Candidate, "ArcTactics", "Candidate");
			Builder.RouteEvent(RouteId_DecisionPackage, "ArcTactics", "DecisionPackage");
			Builder.RouteEvent(RouteId_Decision, "ArcTactics", "Decision");
			Builder.RouteEvent(RouteId_ActionEntered, "ArcTactics", "ActionEntered");
			Builder.RouteEvent(RouteId_ActionStepChanged, "ArcTactics", "ActionStepChanged");
			Builder.RouteEvent(RouteId_ActionEnded, "ArcTactics", "ActionEnded");
			Builder.RouteEvent(RouteId_PathRequested, "ArcTactics", "PathRequested");
			Builder.RouteEvent(RouteId_TQSQuery, "ArcTactics", "TQSQuery");
		}

		virtual bool OnEvent(const uint16 RouteId, const EStyle Style, const FOnEventContext& Context) override
		{
			if (Style != EStyle::Normal)
			{
				return true;
			}
			const FEventData& EventData = Context.EventData;
			const uint64 WorldId = EventData.GetValue<uint64>("WorldId");
			const double WorldTime = EventData.GetValue<double>("WorldTime");
			if (RouteId == RouteId_RecordingStarted)
			{
				OnRecordingStarted(WorldId, WorldTime);
				return true;
			}

			NoteWorld(WorldId, WorldTime);
			FEntityKey Key;
			Key.WorldId = WorldId;
			Key.EntityIndex = EventData.GetValue<uint32>("EntityIndex");
			Key.EntitySerial = EventData.GetValue<uint32>("EntitySerial");
			const int32 EntityIndex = FindOrAddEntity(Key);
			FArcTacticsRecordedEntity& Entity = Recording.Entities[EntityIndex];
			FEntityState& State = EntityStates[EntityIndex];

			switch (RouteId)
			{
			case RouteId_Candidate:
				OnCandidate(State, EventData, WorldTime);
				break;
			case RouteId_DecisionPackage:
				OnDecisionPackage(Entity, State, EventData);
				break;
			case RouteId_Decision:
				OnDecision(Entity, State, EventData, WorldTime);
				break;
			case RouteId_ActionEntered:
				OnActionEntered(Entity, State, EventData, WorldTime);
				break;
			case RouteId_ActionStepChanged:
				OnActionStepChanged(Entity, State, EventData, WorldTime);
				break;
			case RouteId_ActionEnded:
				OnActionEnded(Entity, State, EventData, WorldTime);
				break;
			case RouteId_PathRequested:
				OnPathRequested(Entity, State, EventData, WorldTime);
				break;
			case RouteId_TQSQuery:
				OnTQSQuery(Entity, State, EventData, WorldTime);
				break;
			default:
				break;
			}
			return true;
		}

		virtual void OnAnalysisEnd() override
		{
			TMap<uint64, double> Origins;
			for (const FArcTacticsRecordedWorld& World : Recording.Worlds)
			{
				Origins.Add(World.WorldId, World.OriginWorldTime);
			}

			for (int32 EntityIndex = 0; EntityIndex < Recording.Entities.Num(); ++EntityIndex)
			{
				FArcTacticsRecordedEntity& Entity = Recording.Entities[EntityIndex];
				FEntityState& State = EntityStates[EntityIndex];
				TArray<int32> AssemblingIds;
				State.AssemblingQueries.GetKeys(AssemblingIds);
				for (const int32 QueryId : AssemblingIds)
				{
					State.AssemblingQueries[QueryId].Query.bComplete = false;
					FinalizeQuery(Entity, State, QueryId);
				}
			}

			for (int32 EntityIndex = 0; EntityIndex < Recording.Entities.Num(); ++EntityIndex)
			{
				FArcTacticsRecordedEntity& Entity = Recording.Entities[EntityIndex];
				const int32 OpenSpanIndex = EntityStates[EntityIndex].OpenSpanIndex;
				if (Entity.Actions.IsValidIndex(OpenSpanIndex))
				{
					Entity.Actions[OpenSpanIndex].bOpenAtRecordingEnd = true;
				}
			}

			double Duration = 0.0;
			for (FArcTacticsRecordedEntity& Entity : Recording.Entities)
			{
				const double Origin = Origins.FindRef(Entity.WorldId);
				for (FArcTacticsRecordedDecision& Decision : Entity.Decisions)
				{
					Decision.Time -= Origin;
					Duration = FMath::Max(Duration, Decision.Time);
					for (FArcTacticsRecordedCandidate& Candidate : Decision.Candidates)
					{
						Candidate.Time -= Origin;
					}
				}
				for (FArcTacticsRecordedActionEvent& Event : Entity.Events)
				{
					Event.Time -= Origin;
					Duration = FMath::Max(Duration, Event.Time);
				}
				for (FArcTacticsRecordedPath& Path : Entity.Paths)
				{
					Path.Time -= Origin;
					Duration = FMath::Max(Duration, Path.Time);
				}
				for (FArcTacticsRecordedQuery& Query : Entity.Queries)
				{
					Query.Time -= Origin;
					Duration = FMath::Max(Duration, Query.Time);
				}
				for (FArcTacticsRecordedActionSpan& Span : Entity.Actions)
				{
					Span.StartTime = Span.bEnteredInRecording ? Span.StartTime - Origin : 0.0;
					if (!Span.bOpenAtRecordingEnd)
					{
						Span.EndTime -= Origin;
					}
				}
			}

			for (FArcTacticsRecordedEntity& Entity : Recording.Entities)
			{
				for (FArcTacticsRecordedActionSpan& Span : Entity.Actions)
				{
					if (Span.bOpenAtRecordingEnd)
					{
						Span.EndTime = Duration;
					}
				}
			}

			for (const FEntityState& State : EntityStates)
			{
				Recording.UnclaimedCandidateCount += State.PendingCandidates.Num();
				Recording.UnclaimedQueryCount += State.PendingQueryIndices.Num();
			}
			Recording.Duration = Duration;
		}

	private:
		void OnRecordingStarted(const uint64 WorldId, const double WorldTime)
		{
			if (const int32* Existing = WorldLookup.Find(WorldId))
			{
				FArcTacticsRecordedWorld& World = Recording.Worlds[*Existing];
				if (!World.bHasRecordingStarted)
				{
					World.OriginWorldTime = WorldTime;
					World.bHasRecordingStarted = true;
				}
				return;
			}
			FArcTacticsRecordedWorld& World = Recording.Worlds.AddDefaulted_GetRef();
			World.WorldId = WorldId;
			World.OriginWorldTime = WorldTime;
			World.bHasRecordingStarted = true;
			WorldLookup.Add(WorldId, Recording.Worlds.Num() - 1);
		}

		void NoteWorld(const uint64 WorldId, const double WorldTime)
		{
			if (WorldLookup.Contains(WorldId))
			{
				return;
			}
			FArcTacticsRecordedWorld& World = Recording.Worlds.AddDefaulted_GetRef();
			World.WorldId = WorldId;
			World.OriginWorldTime = WorldTime;
			WorldLookup.Add(WorldId, Recording.Worlds.Num() - 1);
		}

		int32 FindOrAddEntity(const FEntityKey& Key)
		{
			if (const int32* Existing = EntityLookup.Find(Key))
			{
				return *Existing;
			}
			FArcTacticsRecordedEntity& Entity = Recording.Entities.AddDefaulted_GetRef();
			Entity.WorldId = Key.WorldId;
			Entity.EntityIndex = Key.EntityIndex;
			Entity.EntitySerial = Key.EntitySerial;
			EntityStates.AddDefaulted();
			const int32 Index = Recording.Entities.Num() - 1;
			EntityLookup.Add(Key, Index);
			return Index;
		}

		void OnCandidate(FEntityState& State, const FEventData& EventData, const double WorldTime)
		{
			FArcTacticsRecordedCandidate& Candidate = State.PendingCandidates.AddDefaulted_GetRef();
			Candidate.Time = WorldTime;
			Candidate.DecisionGeneration = EventData.GetValue<uint32>("DecisionGeneration");
			Candidate.Order = EventData.GetValue<int32>("Order", INDEX_NONE);
			Candidate.Package = ReadString(EventData, "Package");
			Candidate.Name = ReadString(EventData, "Name");
			Candidate.SourceIndex = EventData.GetValue<int32>("SourceIndex", INDEX_NONE);
			Candidate.bFromSelector = EventData.GetValue<bool>("bFromSelector");
			Candidate.Target = ReadTarget(EventData);
			Candidate.Score = EventData.GetValue<float>("Score");
			Candidate.Priority = EventData.GetValue<int32>("Priority");
			Candidate.bHasPriority = EventData.GetValue<bool>("bHasPriority");
			Candidate.Outcome = static_cast<EArcTacticsCandidateOutcome>(EventData.GetValue<uint8>("Outcome"));
			Candidate.Reason = ReadString(EventData, "Reason");
			const FString ConsiderationNames = ReadString(EventData, "ConsiderationNames");
			if (!ConsiderationNames.IsEmpty())
			{
				ConsiderationNames.ParseIntoArray(Candidate.ConsiderationNames, TEXT("\n"), false);
			}
			Candidate.Raw = ReadFloatArray(EventData, "Raw");
			Candidate.Curved = ReadFloatArray(EventData, "Curved");
			Candidate.EarlyOutIndex = EventData.GetValue<int32>("EarlyOutIndex", INDEX_NONE);
			Candidate.VetoIndex = EventData.GetValue<int32>("VetoIndex", INDEX_NONE);
		}

		void OnDecisionPackage(FArcTacticsRecordedEntity& Entity, FEntityState& State, const FEventData& EventData)
		{
			const uint32 DecisionGeneration = EventData.GetValue<uint32>("DecisionGeneration");
			if (!State.PendingPackages.IsEmpty() && State.PendingPackages.Last().DecisionGeneration != DecisionGeneration)
			{
				State.PendingPackages.Reset();
			}
			FArcTacticsRecordedDecisionPackage& Package = State.PendingPackages.AddDefaulted_GetRef();
			Package.DecisionGeneration = DecisionGeneration;
			Package.ConfigName = ReadString(EventData, "ConfigName");
			Package.MinScore = EventData.GetValue<float>("MinScore");
			Package.bActive = EventData.GetValue<bool>("Active");
			Package.bExcluded = EventData.GetValue<bool>("Excluded");
			Package.Diagnostic = ReadString(EventData, "Diagnostic");
			if (!Package.ConfigName.IsEmpty())
			{
				Entity.ConfigNames.AddUnique(Package.ConfigName);
			}
		}

		void OnDecision(FArcTacticsRecordedEntity& Entity, FEntityState& State, const FEventData& EventData, const double WorldTime)
		{
			FArcTacticsRecordedDecision& Decision = Entity.Decisions.AddDefaulted_GetRef();
			Decision.Time = WorldTime;
			Decision.DecisionGeneration = EventData.GetValue<uint32>("DecisionGeneration");
			Decision.SelectionMode = static_cast<EArcTacticsSelectionMode>(EventData.GetValue<uint8>("SelectionMode"));
			for (const FArcTacticsRecordedDecisionPackage& Package : State.PendingPackages)
			{
				if (Package.DecisionGeneration == Decision.DecisionGeneration)
				{
					Decision.Packages.Add(Package);
				}
			}
			State.PendingPackages.Reset();
			Decision.SelectedOrder = EventData.GetValue<int32>("SelectedOrder", INDEX_NONE);
			Decision.Status = static_cast<EArcTacticsDecisionStatus>(EventData.GetValue<uint8>("Status"));
			Decision.Diagnostic = ReadString(EventData, "Diagnostic");
			Decision.Position = ReadPosition(EventData);
			Decision.Candidates = MoveTemp(State.PendingCandidates);
			State.PendingCandidates.Reset();
			const int32 DecisionIndex = Entity.Decisions.Num() - 1;
			TArray<int32> RemainingQueryIndices;
			for (const int32 QueryIndex : State.PendingQueryIndices)
			{
				FArcTacticsRecordedQuery& Query = Entity.Queries[QueryIndex];
				if (Query.DecisionGeneration == Decision.DecisionGeneration)
				{
					Query.DecisionIndex = DecisionIndex;
					Decision.QueryIndices.Add(QueryIndex);
				}
				else
				{
					RemainingQueryIndices.Add(QueryIndex);
				}
			}
			State.PendingQueryIndices = MoveTemp(RemainingQueryIndices);
		}

		int32 OpenImplicitSpan(FArcTacticsRecordedEntity& Entity, FEntityState& State)
		{
			Entity.Actions.AddDefaulted();
			State.OpenSpanIndex = Entity.Actions.Num() - 1;
			return State.OpenSpanIndex;
		}

		int32 ResolveSpanForSerial(FArcTacticsRecordedEntity& Entity, FEntityState& State, const uint32 ActionSerial)
		{
			if (Entity.Actions.IsValidIndex(State.OpenSpanIndex) && Entity.Actions[State.OpenSpanIndex].ActionSerial == ActionSerial)
			{
				return State.OpenSpanIndex;
			}
			if (ActionSerial != 0)
			{
				for (int32 SpanIndex = Entity.Actions.Num() - 1; SpanIndex >= 0; --SpanIndex)
				{
					if (Entity.Actions[SpanIndex].ActionSerial == ActionSerial)
					{
						return SpanIndex;
					}
				}
			}
			return Entity.Actions.IsValidIndex(State.OpenSpanIndex) ? State.OpenSpanIndex : OpenImplicitSpan(Entity, State);
		}

		void OnActionEntered(FArcTacticsRecordedEntity& Entity, FEntityState& State, const FEventData& EventData, const double WorldTime)
		{
			if (Entity.Actions.IsValidIndex(State.OpenSpanIndex))
			{
				Entity.Actions[State.OpenSpanIndex].EndTime = WorldTime;
			}
			FArcTacticsRecordedActionSpan& Span = Entity.Actions.AddDefaulted_GetRef();
			State.OpenSpanIndex = Entity.Actions.Num() - 1;
			Span.StartTime = WorldTime;
			Span.bEnteredInRecording = true;
			Span.ActionSerial = EventData.GetValue<uint32>("ActionSerial");
			Span.Package = ReadString(EventData, "Package");
			Span.Name = ReadString(EventData, "Name");
			Span.SourceIndex = EventData.GetValue<int32>("SourceIndex", INDEX_NONE);
			Span.bFromSelector = EventData.GetValue<bool>("bFromSelector");
			Span.Target = ReadTarget(EventData);
			Span.StartPosition = ReadPosition(EventData);

			FArcTacticsRecordedActionEvent& Event = Entity.Events.AddDefaulted_GetRef();
			Event.Type = EArcTacticsRecordedActionEventType::Entered;
			Event.Time = WorldTime;
			Event.SpanIndex = State.OpenSpanIndex;
			Event.Position = Span.StartPosition;
		}

		void OnActionStepChanged(FArcTacticsRecordedEntity& Entity, FEntityState& State, const FEventData& EventData, const double WorldTime)
		{
			const int32 SpanIndex = ResolveSpanForSerial(Entity, State, EventData.GetValue<uint32>("ActionSerial"));
			FArcTacticsRecordedActionEvent& Event = Entity.Events.AddDefaulted_GetRef();
			Event.Type = EArcTacticsRecordedActionEventType::StepChanged;
			Event.Time = WorldTime;
			Event.SpanIndex = SpanIndex;
			Event.Position = ReadPosition(EventData);
			Event.StepIndex = EventData.GetValue<int32>("StepIndex", INDEX_NONE);
			Event.StepCount = EventData.GetValue<int32>("StepCount");
			Event.StepName = ReadString(EventData, "StepName");
			Event.StepTypeName = ReadString(EventData, "StepTypeName");
		}

		void OnActionEnded(FArcTacticsRecordedEntity& Entity, FEntityState& State, const FEventData& EventData, const double WorldTime)
		{
			const int32 SpanIndex = Entity.Actions.IsValidIndex(State.OpenSpanIndex) ? State.OpenSpanIndex : OpenImplicitSpan(Entity, State);
			FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
			Span.EndTime = WorldTime;
			Span.bEnded = true;
			if (Span.Package.IsEmpty())
			{
				Span.Package = ReadString(EventData, "Package");
			}
			Span.End.Status = static_cast<EArcTacticsActionEndStatus>(EventData.GetValue<uint8>("Status"));
			Span.End.Reason = static_cast<EArcTacticsActionEndReason>(EventData.GetValue<uint8>("Reason"));
			Span.End.bCooldownStamped = EventData.GetValue<bool>("bCooldownStamped");
			Span.End.Cooldown = EventData.GetValue<float>("Cooldown");
			Span.End.HistoryTag = ReadString(EventData, "HistoryTag");
			Span.End.bHistoryAppended = EventData.GetValue<bool>("bHistoryAppended");
			Span.EndPosition = ReadPosition(EventData);
			State.OpenSpanIndex = INDEX_NONE;

			FArcTacticsRecordedActionEvent& Event = Entity.Events.AddDefaulted_GetRef();
			Event.Type = EArcTacticsRecordedActionEventType::Ended;
			Event.Time = WorldTime;
			Event.SpanIndex = SpanIndex;
			Event.Position = Span.EndPosition;
		}

		void OnPathRequested(FArcTacticsRecordedEntity& Entity, FEntityState& State, const FEventData& EventData, const double WorldTime)
		{
			const int32 SpanIndex = ResolveSpanForSerial(Entity, State, EventData.GetValue<uint32>("ActionSerial"));
			FArcTacticsRecordedPath& Path = Entity.Paths.AddDefaulted_GetRef();
			const int32 PathIndex = Entity.Paths.Num() - 1;
			Path.Time = WorldTime;
			Path.SpanIndex = SpanIndex;
			Path.bRepath = EventData.GetValue<bool>("bRepath");
			Path.bSucceeded = EventData.GetValue<bool>("bSucceeded");
			Path.bPartial = EventData.GetValue<bool>("bPartial");
			Path.Start = ReadVector(EventData, "StartX", "StartY", "StartZ");
			Path.End = ReadVector(EventData, "DestinationX", "DestinationY", "DestinationZ");
			const TArrayView<const float> PointsX = EventData.GetArrayView<float>("PointsX");
			const TArrayView<const float> PointsY = EventData.GetArrayView<float>("PointsY");
			const TArrayView<const float> PointsZ = EventData.GetArrayView<float>("PointsZ");
			const int32 PointCount = FMath::Min3(PointsX.Num(), PointsY.Num(), PointsZ.Num());
			Path.Points.Reserve(PointCount);
			for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				Path.Points.Emplace(PointsX[PointIndex], PointsY[PointIndex], PointsZ[PointIndex]);
			}
			Entity.Actions[SpanIndex].PathIndices.Add(PathIndex);

			FArcTacticsRecordedActionEvent& Event = Entity.Events.AddDefaulted_GetRef();
			Event.Type = EArcTacticsRecordedActionEventType::Path;
			Event.Time = WorldTime;
			Event.SpanIndex = SpanIndex;
			Event.Position.Location = Path.Start;
			Event.Position.bValid = true;
			Event.PathIndex = PathIndex;
		}

		void OnTQSQuery(FArcTacticsRecordedEntity& Entity, FEntityState& State, const FEventData& EventData, const double WorldTime)
		{
			const int32 QueryId = EventData.GetValue<int32>("QueryId", INDEX_NONE);
			const int32 ChunkIndex = EventData.GetValue<int32>("ChunkIndex");
			if (ChunkIndex == 0)
			{
				if (FQueryAssembly* Stale = State.AssemblingQueries.Find(QueryId))
				{
					Stale->Query.bComplete = false;
					FinalizeQuery(Entity, State, QueryId);
				}
				BeginQuery(State.AssemblingQueries.Add(QueryId), EventData, WorldTime);
			}

			FQueryAssembly* Assembly = State.AssemblingQueries.Find(QueryId);
			if (!Assembly)
			{
				return;
			}
			if (ChunkIndex != Assembly->NextChunkIndex || !AppendChunk(*Assembly, EventData))
			{
				Assembly->Query.bComplete = false;
				FinalizeQuery(Entity, State, QueryId);
				return;
			}
			++Assembly->NextChunkIndex;
			if (Assembly->NextChunkIndex >= Assembly->ChunkCount)
			{
				Assembly->Query.bComplete = Assembly->Query.bComplete && Assembly->Query.Items.Num() == Assembly->Query.TotalItemCount;
				FinalizeQuery(Entity, State, QueryId);
			}
		}

		static void BeginQuery(FQueryAssembly& Assembly, const FEventData& EventData, const double WorldTime)
		{
			Assembly.ChunkCount = EventData.GetValue<int32>("ChunkCount");
			FArcTacticsRecordedQuery& Query = Assembly.Query;
			Query.Time = WorldTime;
			Query.DecisionGeneration = EventData.GetValue<uint32>("DecisionGeneration");
			Query.SourceKind = static_cast<EArcTacticsQuerySource>(EventData.GetValue<uint8>("SourceKind"));
			Query.SourceName = ReadString(EventData, "SourceName");
			Query.ActionSerial = EventData.GetValue<uint32>("ActionSerial");
			Query.QueryName = ReadString(EventData, "QueryName");
			Query.QueryId = EventData.GetValue<int32>("QueryId", INDEX_NONE);
			Query.Status = static_cast<EArcTQSQueryStatus>(EventData.GetValue<uint8>("Status"));
			Query.SelectionMode = static_cast<EArcTQSSelectionMode>(EventData.GetValue<uint8>("SelectionMode"));
			Query.TotalItemCount = EventData.GetValue<int32>("TotalItemCount");
			Query.bComplete = true;

			const TArrayView<const float> Querier = EventData.GetArrayView<float>("QuerierLocation");
			if (Querier.Num() >= 3)
			{
				Query.QuerierLocation.Location = FVector3f(Querier[0], Querier[1], Querier[2]);
				Query.QuerierLocation.bValid = true;
			}

			const TArrayView<const uint8> StepTypes = EventData.GetArrayView<uint8>("StepTypes");
			const TArrayView<const int32> StepFilteredCounts = EventData.GetArrayView<int32>("StepFilteredCounts");
			TArray<FString> StepNames;
			const FString JoinedStepNames = ReadString(EventData, "StepNames");
			if (!JoinedStepNames.IsEmpty())
			{
				JoinedStepNames.ParseIntoArray(StepNames, TEXT("\n"), false);
			}
			Query.Steps.SetNum(StepTypes.Num());
			for (int32 StepIndex = 0; StepIndex < StepTypes.Num(); ++StepIndex)
			{
				FArcTacticsRecordedQueryStep& Step = Query.Steps[StepIndex];
				Step.Type = static_cast<EArcTQSStepType>(StepTypes[StepIndex]);
				Step.Name = StepNames.IsValidIndex(StepIndex) ? StepNames[StepIndex] : FString();
				Step.FilteredCount = StepFilteredCounts.IsValidIndex(StepIndex) ? StepFilteredCounts[StepIndex] : 0;
				Step.RawScores.Reserve(Query.TotalItemCount);
				Step.CumulativeScores.Reserve(Query.TotalItemCount);
			}
			Query.Items.Reserve(Query.TotalItemCount);
		}

		static bool AppendChunk(FQueryAssembly& Assembly, const FEventData& EventData)
		{
			FArcTacticsRecordedQuery& Query = Assembly.Query;
			if (EventData.GetValue<int32>("ItemOffset") != Query.Items.Num())
			{
				return false;
			}
			const TArrayView<const float> Scores = EventData.GetArrayView<float>("ItemScores");
			const TArrayView<const float> Locations = EventData.GetArrayView<float>("ItemLocations");
			const TArrayView<const uint8> Valid = EventData.GetArrayView<uint8>("ItemValid");
			const TArrayView<const uint8> Selected = EventData.GetArrayView<uint8>("ItemSelected");
			const TArrayView<const uint8> Applied = EventData.GetArrayView<uint8>("ItemApplied");
			const TArrayView<const uint32> EntityIndices = EventData.GetArrayView<uint32>("ItemEntityIndices");
			const TArrayView<const uint32> EntitySerials = EventData.GetArrayView<uint32>("ItemEntitySerials");
			const int32 ChunkItems = Scores.Num();
			if (Locations.Num() != ChunkItems * 3 || Valid.Num() != ChunkItems || Selected.Num() != ChunkItems
				|| Applied.Num() != ChunkItems || EntityIndices.Num() != ChunkItems || EntitySerials.Num() != ChunkItems)
			{
				return false;
			}

			Query.Items.Reserve(Query.Items.Num() + ChunkItems);
			for (int32 ItemIndex = 0; ItemIndex < ChunkItems; ++ItemIndex)
			{
				FArcTacticsRecordedQueryItem& Item = Query.Items.AddDefaulted_GetRef();
				Item.Location = FVector3f(Locations[ItemIndex * 3], Locations[ItemIndex * 3 + 1], Locations[ItemIndex * 3 + 2]);
				Item.Score = Scores[ItemIndex];
				Item.EntityIndex = EntityIndices[ItemIndex];
				Item.EntitySerial = EntitySerials[ItemIndex];
				Item.bValid = Valid[ItemIndex] != 0;
				Item.bSelected = Selected[ItemIndex] != 0;
				Item.bApplied = Applied[ItemIndex] != 0;
			}

			const TArrayView<const float> RawScores = EventData.GetArrayView<float>("StepRawScores");
			const TArrayView<const float> CumulativeScores = EventData.GetArrayView<float>("StepCumulativeScores");
			const int32 StepCount = Query.Steps.Num();
			if (RawScores.Num() != StepCount * ChunkItems || CumulativeScores.Num() != StepCount * ChunkItems)
			{
				Query.bComplete = false;
			}
			for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
			{
				FArcTacticsRecordedQueryStep& Step = Query.Steps[StepIndex];
				for (int32 ItemIndex = 0; ItemIndex < ChunkItems; ++ItemIndex)
				{
					const int32 FlatIndex = StepIndex * ChunkItems + ItemIndex;
					Step.RawScores.Add(RawScores.IsValidIndex(FlatIndex) ? RawScores[FlatIndex] : 0.0f);
					Step.CumulativeScores.Add(CumulativeScores.IsValidIndex(FlatIndex) ? CumulativeScores[FlatIndex] : 0.0f);
				}
			}
			return true;
		}

		void FinalizeQuery(FArcTacticsRecordedEntity& Entity, FEntityState& State, const int32 QueryId)
		{
			FQueryAssembly Assembly;
			State.AssemblingQueries.RemoveAndCopyValue(QueryId, Assembly);
			const int32 QueryIndex = Entity.Queries.Add(MoveTemp(Assembly.Query));
			FArcTacticsRecordedQuery& Query = Entity.Queries[QueryIndex];
			if (Query.SourceKind != EArcTacticsQuerySource::Step)
			{
				State.PendingQueryIndices.Add(QueryIndex);
				return;
			}

			const int32 SpanIndex = ResolveSpanForSerial(Entity, State, Query.ActionSerial);
			Query.SpanIndex = SpanIndex;
			Entity.Actions[SpanIndex].QueryIndices.Add(QueryIndex);

			FArcTacticsRecordedActionEvent& Event = Entity.Events.AddDefaulted_GetRef();
			Event.Type = EArcTacticsRecordedActionEventType::Query;
			Event.Time = Query.Time;
			Event.SpanIndex = SpanIndex;
			Event.Position = Query.QuerierLocation;
			Event.QueryIndex = QueryIndex;
		}

		FArcTacticsRecording& Recording;
		TMap<FEntityKey, int32> EntityLookup;
		TArray<FEntityState> EntityStates;
		TMap<uint64, int32> WorldLookup;
	};
}

struct FArcTacticsTraceLoader::FImpl
{
	TUniquePtr<FArcTacticsRecording> Recording;
	TUniquePtr<UE::Trace::FFileDataStream> Stream;
	TUniquePtr<ArcTacticsTraceLoaderPrivate::FAnalyzer> Analyzer;
	UE::Trace::FAnalysisProcessor Processor;
	bool bResultTaken = false;
};

FArcTacticsTraceLoader::FArcTacticsTraceLoader() = default;

FArcTacticsTraceLoader::~FArcTacticsTraceLoader()
{
	Cancel();
}

bool FArcTacticsTraceLoader::Start(const FString& FilePath, FString& OutError)
{
	Cancel();

	TUniquePtr<FImpl> NewImpl = MakeUnique<FImpl>();
	NewImpl->Recording = MakeUnique<FArcTacticsRecording>();
	NewImpl->Recording->FilePath = FilePath;
	NewImpl->Stream = MakeUnique<UE::Trace::FFileDataStream>();
	if (!NewImpl->Stream->Open(*FilePath))
	{
		OutError = FString::Printf(TEXT("Cannot open %s"), *FilePath);
		return false;
	}
	NewImpl->Analyzer = MakeUnique<ArcTacticsTraceLoaderPrivate::FAnalyzer>(*NewImpl->Recording);

	UE::Trace::FAnalysisContext Context;
	Context.AddAnalyzer(*NewImpl->Analyzer);
	NewImpl->Processor = Context.Process(*NewImpl->Stream);
	Impl = MoveTemp(NewImpl);
	return true;
}

bool FArcTacticsTraceLoader::IsLoading() const
{
	return Impl.IsValid() && !Impl->bResultTaken && Impl->Processor.IsActive();
}

bool FArcTacticsTraceLoader::IsFinished() const
{
	return Impl.IsValid() && !Impl->bResultTaken && !Impl->Processor.IsActive();
}

TUniquePtr<FArcTacticsRecording> FArcTacticsTraceLoader::TakeResult()
{
	if (!IsFinished())
	{
		return nullptr;
	}
	Impl->Processor.Wait();
	Impl->bResultTaken = true;
	// The analysis thread can still be unwinding after IsActive() turns false (Processor.cpp:71-72,93-97), so the processor, analyzer and stream stay alive until the next Start or Cancel.
	return MoveTemp(Impl->Recording);
}

void FArcTacticsTraceLoader::Cancel()
{
	if (!Impl.IsValid())
	{
		return;
	}
	Impl->Processor.Stop();
	Impl->Processor.Wait();
	Impl.Reset();
}

TUniquePtr<FArcTacticsRecording> FArcTacticsTraceLoader::LoadBlocking(const FString& FilePath, FString& OutError)
{
	FArcTacticsTraceLoader Loader;
	if (!Loader.Start(FilePath, OutError))
	{
		return nullptr;
	}
	Loader.Impl->Processor.Wait();
	TUniquePtr<FArcTacticsRecording> Recording = Loader.TakeResult();
	if (!Recording.IsValid())
	{
		OutError = FString::Printf(TEXT("Analysis of %s produced no recording"), *FilePath);
		return nullptr;
	}
	if (Recording->Entities.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s contains no ArcTactics entity (not a Tactics recording, or empty)"), *FilePath);
		return nullptr;
	}
	return Recording;
}

#else

struct FArcTacticsTraceLoader::FImpl
{
};

FArcTacticsTraceLoader::FArcTacticsTraceLoader() = default;

FArcTacticsTraceLoader::~FArcTacticsTraceLoader() = default;

bool FArcTacticsTraceLoader::Start(const FString& FilePath, FString& OutError)
{
	OutError = TEXT("Trace analysis unavailable in this configuration");
	return false;
}

bool FArcTacticsTraceLoader::IsLoading() const
{
	return false;
}

bool FArcTacticsTraceLoader::IsFinished() const
{
	return false;
}

TUniquePtr<FArcTacticsRecording> FArcTacticsTraceLoader::TakeResult()
{
	return nullptr;
}

void FArcTacticsTraceLoader::Cancel()
{
}

TUniquePtr<FArcTacticsRecording> FArcTacticsTraceLoader::LoadBlocking(const FString& FilePath, FString& OutError)
{
	OutError = TEXT("Trace analysis unavailable in this configuration");
	return nullptr;
}

#endif
