// Copyright Lukasz Baran. All Rights Reserved.

#include "Debugger/ArcTacticsTrace.h"

#if WITH_ARCTACTICS_TRACE

#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryInstance.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsNode.h"
#include "Engine/World.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"
#include "HAL/PlatformTime.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "NavigationData.h"
#include "ObjectTrace.h"
#include "Trace/Detail/Protocol.h"
#include "Trace/Trace.inl"
#include "UtilityAI/ArcUtilityScoring.h"

UE_TRACE_CHANNEL_DEFINE(ArcTacticsChannel)

UE_TRACE_EVENT_BEGIN(ArcTactics, RecordingStarted)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTactics, Candidate)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
	UE_TRACE_EVENT_FIELD(uint32, DecisionGeneration)
	UE_TRACE_EVENT_FIELD(int32, Order)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Package)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
	UE_TRACE_EVENT_FIELD(int32, SourceIndex)
	UE_TRACE_EVENT_FIELD(bool, bFromSelector)
	UE_TRACE_EVENT_FIELD(uint8, IdentityType)
	UE_TRACE_EVENT_FIELD(uint32, TargetIndex)
	UE_TRACE_EVENT_FIELD(uint32, TargetSerial)
	UE_TRACE_EVENT_FIELD(float, TargetX)
	UE_TRACE_EVENT_FIELD(float, TargetY)
	UE_TRACE_EVENT_FIELD(float, TargetZ)
	UE_TRACE_EVENT_FIELD(bool, bHasTargetLocation)
	UE_TRACE_EVENT_FIELD(float, Score)
	UE_TRACE_EVENT_FIELD(int32, Priority)
	UE_TRACE_EVENT_FIELD(bool, bHasPriority)
	UE_TRACE_EVENT_FIELD(uint8, Outcome)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Reason)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, ConsiderationNames)
	UE_TRACE_EVENT_FIELD(float[], Raw)
	UE_TRACE_EVENT_FIELD(float[], Curved)
	UE_TRACE_EVENT_FIELD(int32, EarlyOutIndex)
	UE_TRACE_EVENT_FIELD(int32, VetoIndex)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTactics, DecisionPackage)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
	UE_TRACE_EVENT_FIELD(uint32, DecisionGeneration)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, ConfigName)
	UE_TRACE_EVENT_FIELD(float, MinScore)
	UE_TRACE_EVENT_FIELD(bool, Active)
	UE_TRACE_EVENT_FIELD(bool, Excluded)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Diagnostic)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTactics, Decision)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
	UE_TRACE_EVENT_FIELD(uint32, DecisionGeneration)
	UE_TRACE_EVENT_FIELD(uint8, SelectionMode)
	UE_TRACE_EVENT_FIELD(int32, SelectedOrder)
	UE_TRACE_EVENT_FIELD(uint8, Status)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Diagnostic)
	UE_TRACE_EVENT_FIELD(bool, bHasLocation)
	UE_TRACE_EVENT_FIELD(float, LocationX)
	UE_TRACE_EVENT_FIELD(float, LocationY)
	UE_TRACE_EVENT_FIELD(float, LocationZ)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTactics, ActionEntered)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
	UE_TRACE_EVENT_FIELD(uint32, ActionSerial)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Package)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
	UE_TRACE_EVENT_FIELD(int32, SourceIndex)
	UE_TRACE_EVENT_FIELD(bool, bFromSelector)
	UE_TRACE_EVENT_FIELD(uint8, IdentityType)
	UE_TRACE_EVENT_FIELD(uint32, TargetIndex)
	UE_TRACE_EVENT_FIELD(uint32, TargetSerial)
	UE_TRACE_EVENT_FIELD(float, TargetX)
	UE_TRACE_EVENT_FIELD(float, TargetY)
	UE_TRACE_EVENT_FIELD(float, TargetZ)
	UE_TRACE_EVENT_FIELD(bool, bHasTargetLocation)
	UE_TRACE_EVENT_FIELD(bool, bHasLocation)
	UE_TRACE_EVENT_FIELD(float, LocationX)
	UE_TRACE_EVENT_FIELD(float, LocationY)
	UE_TRACE_EVENT_FIELD(float, LocationZ)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTactics, ActionStepChanged)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
	UE_TRACE_EVENT_FIELD(uint32, ActionSerial)
	UE_TRACE_EVENT_FIELD(int32, StepIndex)
	UE_TRACE_EVENT_FIELD(int32, StepCount)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, StepName)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, StepTypeName)
	UE_TRACE_EVENT_FIELD(bool, bHasLocation)
	UE_TRACE_EVENT_FIELD(float, LocationX)
	UE_TRACE_EVENT_FIELD(float, LocationY)
	UE_TRACE_EVENT_FIELD(float, LocationZ)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTactics, ActionEnded)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Package)
	UE_TRACE_EVENT_FIELD(uint8, Status)
	UE_TRACE_EVENT_FIELD(uint8, Reason)
	UE_TRACE_EVENT_FIELD(bool, bCooldownStamped)
	UE_TRACE_EVENT_FIELD(float, Cooldown)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, HistoryTag)
	UE_TRACE_EVENT_FIELD(bool, bHistoryAppended)
	UE_TRACE_EVENT_FIELD(bool, bHasLocation)
	UE_TRACE_EVENT_FIELD(float, LocationX)
	UE_TRACE_EVENT_FIELD(float, LocationY)
	UE_TRACE_EVENT_FIELD(float, LocationZ)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTactics, PathRequested)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
	UE_TRACE_EVENT_FIELD(uint32, ActionSerial)
	UE_TRACE_EVENT_FIELD(bool, bRepath)
	UE_TRACE_EVENT_FIELD(bool, bSucceeded)
	UE_TRACE_EVENT_FIELD(bool, bPartial)
	UE_TRACE_EVENT_FIELD(float, StartX)
	UE_TRACE_EVENT_FIELD(float, StartY)
	UE_TRACE_EVENT_FIELD(float, StartZ)
	UE_TRACE_EVENT_FIELD(float, DestinationX)
	UE_TRACE_EVENT_FIELD(float, DestinationY)
	UE_TRACE_EVENT_FIELD(float, DestinationZ)
	UE_TRACE_EVENT_FIELD(float[], PointsX)
	UE_TRACE_EVENT_FIELD(float[], PointsY)
	UE_TRACE_EVENT_FIELD(float[], PointsZ)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTactics, TQSQuery)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
	UE_TRACE_EVENT_FIELD(uint32, DecisionGeneration)
	UE_TRACE_EVENT_FIELD(uint8, SourceKind)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, SourceName)
	UE_TRACE_EVENT_FIELD(uint32, ActionSerial)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, QueryName)
	UE_TRACE_EVENT_FIELD(int32, QueryId)
	UE_TRACE_EVENT_FIELD(uint8, Status)
	UE_TRACE_EVENT_FIELD(uint8, SelectionMode)
	UE_TRACE_EVENT_FIELD(float[], QuerierLocation)
	UE_TRACE_EVENT_FIELD(int32, ChunkIndex)
	UE_TRACE_EVENT_FIELD(int32, ChunkCount)
	UE_TRACE_EVENT_FIELD(int32, ItemOffset)
	UE_TRACE_EVENT_FIELD(int32, TotalItemCount)
	UE_TRACE_EVENT_FIELD(float[], ItemLocations)
	UE_TRACE_EVENT_FIELD(float[], ItemScores)
	UE_TRACE_EVENT_FIELD(uint8[], ItemValid)
	UE_TRACE_EVENT_FIELD(uint8[], ItemSelected)
	UE_TRACE_EVENT_FIELD(uint8[], ItemApplied)
	UE_TRACE_EVENT_FIELD(uint32[], ItemEntityIndices)
	UE_TRACE_EVENT_FIELD(uint32[], ItemEntitySerials)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, StepNames)
	UE_TRACE_EVENT_FIELD(uint8[], StepTypes)
	UE_TRACE_EVENT_FIELD(int32[], StepFilteredCounts)
	UE_TRACE_EVENT_FIELD(float[], StepRawScores)
	UE_TRACE_EVENT_FIELD(float[], StepCumulativeScores)
UE_TRACE_EVENT_END()

namespace ArcTacticsTracePrivate
{
	double GetWorldTime(const UWorld* World)
	{
		return World ? World->GetTimeSeconds() : 0.0;
	}

	uint32 GetTargetIndex(const FArcTacticsCandidateIdentity& Identity)
	{
		return static_cast<uint32>(Identity.Entity.Index);
	}

	uint32 GetTargetSerial(const FArcTacticsCandidateIdentity& Identity)
	{
		return static_cast<uint32>(Identity.Entity.SerialNumber);
	}

	struct FEntityLocation
	{
		FVector3f Location = FVector3f::ZeroVector;
		bool bValid = false;
	};

	FEntityLocation GetEntityLocation(const FMassEntityManager* EntityManager, const FMassEntityHandle Entity)
	{
		FEntityLocation Result;
		if (!EntityManager || !EntityManager->IsEntityActive(Entity))
		{
			return Result;
		}
		const FTransformFragment* Transform = EntityManager->GetFragmentDataPtr<FTransformFragment>(Entity);
		if (!Transform)
		{
			return Result;
		}
		Result.Location = FVector3f(Transform->GetTransform().GetLocation());
		Result.bValid = true;
		return Result;
	}

	FEntityLocation GetTargetLocation(const FMassEntityManager* EntityManager, const FArcTacticsCandidateIdentity& Identity)
	{
		switch (Identity.Type)
		{
		case EArcTacticsValueIdentity::Entity:
			return GetEntityLocation(EntityManager, Identity.Entity);
		case EArcTacticsValueIdentity::Vector:
			return FEntityLocation{FVector3f(Identity.Vector), true};
		default:
			return FEntityLocation();
		}
	}

	FString GetPackageName(const FArcTacticsInstanceSlot& Slot, const int32 PackageIndex)
	{
		return Slot.Packages.IsValidIndex(PackageIndex) && Slot.Packages[PackageIndex]
			? GetNameSafe(Slot.Packages[PackageIndex]->Config)
			: FString();
	}

	constexpr int64 TraceArrayByteLimit = UE::Trace::Protocol7::FAuxHeader::SizeLimit;

	template <typename ElementType>
	bool FitsTraceArray(const int64 Num)
	{
		return Num * static_cast<int64>(sizeof(ElementType)) < TraceArrayByteLimit;
	}

	void AppendLocation(TArray<float>& Out, const FVector& Location)
	{
		Out.Add(static_cast<float>(Location.X));
		Out.Add(static_cast<float>(Location.Y));
		Out.Add(static_cast<float>(Location.Z));
	}

	void MarkResults(
		const FArcTQSQueryInstance& Query,
		const int32 AppliedCount,
		TArray<uint8>& OutSelected,
		TArray<uint8>& OutApplied)
	{
		OutSelected.SetNumZeroed(Query.Items.Num());
		OutApplied.SetNumZeroed(Query.Items.Num());
		for (int32 ResultIndex = 0; ResultIndex < Query.ResultIndices.Num(); ++ResultIndex)
		{
			const int32 ItemIndex = Query.ResultIndices[ResultIndex];
			if (OutSelected.IsValidIndex(ItemIndex))
			{
				OutSelected[ItemIndex] = 1;
				OutApplied[ItemIndex] = ResultIndex < AppliedCount ? 1 : 0;
			}
		}
	}
}

uint64 ArcTacticsTrace::GetWorldId(const UWorld* World)
{
#if OBJECT_TRACE_ENABLED
	const uint64 ObjectId = FObjectTrace::GetObjectId(World);
	if (ObjectId != 0)
	{
		return ObjectId;
	}
#endif
	return static_cast<uint64>(reinterpret_cast<UPTRINT>(World));
}

void ArcTacticsTrace::OutputRecordingStarted(const UWorld* World)
{
	UE_TRACE_LOG(ArcTactics, RecordingStarted, ArcTacticsChannel)
		<< RecordingStarted.Cycle(FPlatformTime::Cycles64())
		<< RecordingStarted.WorldId(GetWorldId(World))
		<< RecordingStarted.WorldTime(ArcTacticsTracePrivate::GetWorldTime(World));
}

void ArcTacticsTrace::OutputCandidate(
	const UWorld* World,
	const FMassEntityManager* EntityManager,
	const FArcTacticsInstanceSlot& Slot,
	const FArcTacticsCompiledData& CompiledData,
	const FArcTacticsCandidate& TracedCandidate,
	const TConstArrayView<FArcTacticsNodeHandle> Considerations,
	const TConstArrayView<FArcUtilityConsiderationSnapshot> Snapshots,
	const int32 EarlyOutIndex,
	const int32 VetoIndex,
	const TOptional<int32> Priority)
{
	const FMassEntityHandle Entity = Slot.Context.Self;
	const FArcTacticsCandidateIdentity& Identity = TracedCandidate.Option.Identity;
	const FString Package = ArcTacticsTracePrivate::GetPackageName(Slot, TracedCandidate.Option.PackageIndex);
	const FString Name = TracedCandidate.Option.Name.ToString();
	const FString Reason = TracedCandidate.RejectionReason.IsNone() ? FString() : TracedCandidate.RejectionReason.ToString();
	const ArcTacticsTracePrivate::FEntityLocation TargetLocation =
		ArcTacticsTracePrivate::GetTargetLocation(EntityManager, Identity);

	TStringBuilder<256> ConsiderationNames;
	for (int32 Index = 0; Index < Considerations.Num(); ++Index)
	{
		if (Index > 0)
		{
			ConsiderationNames.AppendChar(TEXT('\n'));
		}
		const int32 NodeIndex = Considerations[Index].Get();
		if (CompiledData.Nodes.IsValidIndex(NodeIndex))
		{
			ConsiderationNames.Append(CompiledData.Nodes[NodeIndex].Name.ToString());
		}
	}

	TArray<float, TInlineAllocator<16>> Raw;
	TArray<float, TInlineAllocator<16>> Curved;
	Raw.Reserve(Snapshots.Num());
	Curved.Reserve(Snapshots.Num());
	for (const FArcUtilityConsiderationSnapshot& Snapshot : Snapshots)
	{
		Raw.Add(Snapshot.RawScore);
		Curved.Add(Snapshot.CurvedScore);
	}

	UE_TRACE_LOG(ArcTactics, Candidate, ArcTacticsChannel)
		<< Candidate.Cycle(FPlatformTime::Cycles64())
		<< Candidate.WorldId(GetWorldId(World))
		<< Candidate.EntityIndex(static_cast<uint32>(Entity.Index))
		<< Candidate.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< Candidate.WorldTime(ArcTacticsTracePrivate::GetWorldTime(World))
		<< Candidate.DecisionGeneration(Slot.DecisionGeneration)
		<< Candidate.Order(TracedCandidate.Order)
		<< Candidate.Package(*Package, Package.Len())
		<< Candidate.Name(*Name, Name.Len())
		<< Candidate.SourceIndex(TracedCandidate.Option.SourceIndex)
		<< Candidate.bFromSelector(TracedCandidate.Option.bFromSelector)
		<< Candidate.IdentityType(static_cast<uint8>(Identity.Type))
		<< Candidate.TargetIndex(ArcTacticsTracePrivate::GetTargetIndex(Identity))
		<< Candidate.TargetSerial(ArcTacticsTracePrivate::GetTargetSerial(Identity))
		<< Candidate.TargetX(TargetLocation.Location.X)
		<< Candidate.TargetY(TargetLocation.Location.Y)
		<< Candidate.TargetZ(TargetLocation.Location.Z)
		<< Candidate.bHasTargetLocation(TargetLocation.bValid)
		<< Candidate.Score(TracedCandidate.Option.Score)
		<< Candidate.Priority(Priority.Get(0))
		<< Candidate.bHasPriority(Priority.IsSet())
		<< Candidate.Outcome(static_cast<uint8>(TracedCandidate.Outcome))
		<< Candidate.Reason(*Reason, Reason.Len())
		<< Candidate.ConsiderationNames(ConsiderationNames.GetData(), ConsiderationNames.Len())
		<< Candidate.Raw(Raw.GetData(), Raw.Num())
		<< Candidate.Curved(Curved.GetData(), Curved.Num())
		<< Candidate.EarlyOutIndex(EarlyOutIndex)
		<< Candidate.VetoIndex(VetoIndex);
}

void ArcTacticsTrace::OutputDecision(
	const FArcTacticsTaskContext& Context,
	const FArcTacticsInstanceSlot& Slot,
	const int32 SelectedOrder)
{
	const FMassEntityHandle Entity = Slot.Context.Self;
	const uint64 WorldId = GetWorldId(Context.World);
	const double WorldTime = ArcTacticsTracePrivate::GetWorldTime(Context.World);
	for (const TUniquePtr<FArcTacticsPackage>& Package : Slot.Packages)
	{
		const FString ConfigName = GetNameSafe(Package->Config);
		const FString PackageDiagnostic = Package->Instance.Diagnostic.IsNone()
			? FString()
			: Package->Instance.Diagnostic.ToString();
		UE_TRACE_LOG(ArcTactics, DecisionPackage, ArcTacticsChannel)
			<< DecisionPackage.Cycle(FPlatformTime::Cycles64())
			<< DecisionPackage.WorldId(WorldId)
			<< DecisionPackage.EntityIndex(static_cast<uint32>(Entity.Index))
			<< DecisionPackage.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
			<< DecisionPackage.WorldTime(WorldTime)
			<< DecisionPackage.DecisionGeneration(Slot.DecisionGeneration)
			<< DecisionPackage.ConfigName(*ConfigName, ConfigName.Len())
			<< DecisionPackage.MinScore(Package->Instance.DecisionSettings.MinScore)
			<< DecisionPackage.Active(Package->bActive)
			<< DecisionPackage.Excluded(Package->bExcluded)
			<< DecisionPackage.Diagnostic(*PackageDiagnostic, PackageDiagnostic.Len());
	}
	const FString Diagnostic = Slot.Diagnostic.IsNone() ? FString() : Slot.Diagnostic.ToString();
	const ArcTacticsTracePrivate::FEntityLocation Location =
		ArcTacticsTracePrivate::GetEntityLocation(Context.EntityManager, Entity);

	UE_TRACE_LOG(ArcTactics, Decision, ArcTacticsChannel)
		<< Decision.Cycle(FPlatformTime::Cycles64())
		<< Decision.WorldId(WorldId)
		<< Decision.EntityIndex(static_cast<uint32>(Entity.Index))
		<< Decision.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< Decision.WorldTime(WorldTime)
		<< Decision.DecisionGeneration(Slot.DecisionGeneration)
		<< Decision.SelectionMode(static_cast<uint8>(Slot.Settings.Selection.Mode))
		<< Decision.SelectedOrder(SelectedOrder)
		<< Decision.Status(static_cast<uint8>(Slot.DecisionStatus))
		<< Decision.Diagnostic(*Diagnostic, Diagnostic.Len())
		<< Decision.bHasLocation(Location.bValid)
		<< Decision.LocationX(Location.Location.X)
		<< Decision.LocationY(Location.Location.Y)
		<< Decision.LocationZ(Location.Location.Z);
}

void ArcTacticsTrace::OutputActionEntered(
	const UWorld* World,
	const FMassEntityManager* EntityManager,
	const FArcTacticsInstanceSlot& Slot)
{
	const FMassEntityHandle Entity = Slot.Context.Self;
	const FArcTacticsOption& Action = Slot.ActiveAction;
	const FArcTacticsCandidateIdentity& Identity = Action.Identity;
	const FString Package = ArcTacticsTracePrivate::GetPackageName(Slot, Action.PackageIndex);
	const FString Name = Action.Name.ToString();
	const ArcTacticsTracePrivate::FEntityLocation Location =
		ArcTacticsTracePrivate::GetEntityLocation(EntityManager, Entity);
	const ArcTacticsTracePrivate::FEntityLocation TargetLocation =
		ArcTacticsTracePrivate::GetTargetLocation(EntityManager, Identity);

	UE_TRACE_LOG(ArcTactics, ActionEntered, ArcTacticsChannel)
		<< ActionEntered.Cycle(FPlatformTime::Cycles64())
		<< ActionEntered.WorldId(GetWorldId(World))
		<< ActionEntered.EntityIndex(static_cast<uint32>(Entity.Index))
		<< ActionEntered.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< ActionEntered.WorldTime(ArcTacticsTracePrivate::GetWorldTime(World))
		<< ActionEntered.ActionSerial(Action.Serial)
		<< ActionEntered.Package(*Package, Package.Len())
		<< ActionEntered.Name(*Name, Name.Len())
		<< ActionEntered.SourceIndex(Action.SourceIndex)
		<< ActionEntered.bFromSelector(Action.bFromSelector)
		<< ActionEntered.IdentityType(static_cast<uint8>(Identity.Type))
		<< ActionEntered.TargetIndex(ArcTacticsTracePrivate::GetTargetIndex(Identity))
		<< ActionEntered.TargetSerial(ArcTacticsTracePrivate::GetTargetSerial(Identity))
		<< ActionEntered.TargetX(TargetLocation.Location.X)
		<< ActionEntered.TargetY(TargetLocation.Location.Y)
		<< ActionEntered.TargetZ(TargetLocation.Location.Z)
		<< ActionEntered.bHasTargetLocation(TargetLocation.bValid)
		<< ActionEntered.bHasLocation(Location.bValid)
		<< ActionEntered.LocationX(Location.Location.X)
		<< ActionEntered.LocationY(Location.Location.Y)
		<< ActionEntered.LocationZ(Location.Location.Z);
}

void ArcTacticsTrace::OutputActionStepChanged(
	const UWorld* World,
	const FMassEntityManager* EntityManager,
	const FMassEntityHandle Entity,
	const uint32 ActionSerial,
	const int32 StepIndex,
	const int32 StepCount,
	const FName StepName,
	const FArcTacticsStepNode* StepNode)
{
	const FString Name = StepName.IsNone() ? FString() : StepName.ToString();
	const FString TypeName = StepNode ? StepNode->GetDisplayName().ToString() : FString();
	const ArcTacticsTracePrivate::FEntityLocation Location =
		ArcTacticsTracePrivate::GetEntityLocation(EntityManager, Entity);

	UE_TRACE_LOG(ArcTactics, ActionStepChanged, ArcTacticsChannel)
		<< ActionStepChanged.Cycle(FPlatformTime::Cycles64())
		<< ActionStepChanged.WorldId(GetWorldId(World))
		<< ActionStepChanged.EntityIndex(static_cast<uint32>(Entity.Index))
		<< ActionStepChanged.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< ActionStepChanged.WorldTime(ArcTacticsTracePrivate::GetWorldTime(World))
		<< ActionStepChanged.ActionSerial(ActionSerial)
		<< ActionStepChanged.StepIndex(StepIndex)
		<< ActionStepChanged.StepCount(StepCount)
		<< ActionStepChanged.StepName(*Name, Name.Len())
		<< ActionStepChanged.StepTypeName(*TypeName, TypeName.Len())
		<< ActionStepChanged.bHasLocation(Location.bValid)
		<< ActionStepChanged.LocationX(Location.Location.X)
		<< ActionStepChanged.LocationY(Location.Location.Y)
		<< ActionStepChanged.LocationZ(Location.Location.Z);
}

void ArcTacticsTrace::OutputActionEnded(
	const UWorld* World,
	const FMassEntityManager* EntityManager,
	const FArcTacticsInstanceSlot& Slot,
	const EArcTacticsActionEndStatus Status,
	const EArcTacticsActionEndReason Reason,
	const bool bCooldownStamped,
	const bool bHistoryAppended)
{
	if (!Slot.ActiveAction.IsSet())
	{
		return;
	}
	const FMassEntityHandle Entity = Slot.Context.Self;
	const FArcTacticsBranchParameters& Parameters = Slot.ActiveAction.Parameters;
	const FString Package = ArcTacticsTracePrivate::GetPackageName(Slot, Slot.ActiveAction.PackageIndex);
	const FString HistoryTag = Parameters.HistoryTag.IsValid() ? Parameters.HistoryTag.ToString() : FString();
	const ArcTacticsTracePrivate::FEntityLocation Location =
		ArcTacticsTracePrivate::GetEntityLocation(EntityManager, Entity);

	UE_TRACE_LOG(ArcTactics, ActionEnded, ArcTacticsChannel)
		<< ActionEnded.Cycle(FPlatformTime::Cycles64())
		<< ActionEnded.WorldId(GetWorldId(World))
		<< ActionEnded.EntityIndex(static_cast<uint32>(Entity.Index))
		<< ActionEnded.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< ActionEnded.WorldTime(ArcTacticsTracePrivate::GetWorldTime(World))
		<< ActionEnded.Package(*Package, Package.Len())
		<< ActionEnded.Status(static_cast<uint8>(Status))
		<< ActionEnded.Reason(static_cast<uint8>(Reason))
		<< ActionEnded.bCooldownStamped(bCooldownStamped)
		<< ActionEnded.Cooldown(Parameters.Cooldown)
		<< ActionEnded.HistoryTag(*HistoryTag, HistoryTag.Len())
		<< ActionEnded.bHistoryAppended(bHistoryAppended)
		<< ActionEnded.bHasLocation(Location.bValid)
		<< ActionEnded.LocationX(Location.Location.X)
		<< ActionEnded.LocationY(Location.Location.Y)
		<< ActionEnded.LocationZ(Location.Location.Z);
}

void ArcTacticsTrace::OutputPathRequested(
	const UWorld* World,
	const FMassEntityHandle Entity,
	const uint32 ActionSerial,
	const bool bRepath,
	const FVector& Start,
	const FVector& Destination,
	const FNavigationPath* Path)
{
	TArray<float, TInlineAllocator<32>> PointsX;
	TArray<float, TInlineAllocator<32>> PointsY;
	TArray<float, TInlineAllocator<32>> PointsZ;
	if (Path)
	{
		const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
		PointsX.Reserve(Points.Num());
		PointsY.Reserve(Points.Num());
		PointsZ.Reserve(Points.Num());
		for (const FNavPathPoint& Point : Points)
		{
			PointsX.Add(static_cast<float>(Point.Location.X));
			PointsY.Add(static_cast<float>(Point.Location.Y));
			PointsZ.Add(static_cast<float>(Point.Location.Z));
		}
	}

	UE_TRACE_LOG(ArcTactics, PathRequested, ArcTacticsChannel)
		<< PathRequested.Cycle(FPlatformTime::Cycles64())
		<< PathRequested.WorldId(GetWorldId(World))
		<< PathRequested.EntityIndex(static_cast<uint32>(Entity.Index))
		<< PathRequested.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< PathRequested.WorldTime(ArcTacticsTracePrivate::GetWorldTime(World))
		<< PathRequested.ActionSerial(ActionSerial)
		<< PathRequested.bRepath(bRepath)
		<< PathRequested.bSucceeded(Path != nullptr)
		<< PathRequested.bPartial(Path && Path->IsPartial())
		<< PathRequested.StartX(static_cast<float>(Start.X))
		<< PathRequested.StartY(static_cast<float>(Start.Y))
		<< PathRequested.StartZ(static_cast<float>(Start.Z))
		<< PathRequested.DestinationX(static_cast<float>(Destination.X))
		<< PathRequested.DestinationY(static_cast<float>(Destination.Y))
		<< PathRequested.DestinationZ(static_cast<float>(Destination.Z))
		<< PathRequested.PointsX(PointsX.GetData(), PointsX.Num())
		<< PathRequested.PointsY(PointsY.GetData(), PointsY.Num())
		<< PathRequested.PointsZ(PointsZ.GetData(), PointsZ.Num());
}

void ArcTacticsTrace::OutputTQSQuery(
	const UWorld* World,
	const FMassEntityManager* EntityManager,
	const FMassEntityHandle Entity,
	const uint32 DecisionGeneration,
	const EArcTacticsQuerySource SourceKind,
	const FName SourceName,
	const uint32 ActionSerial,
	const FArcTacticsQueryReport& Report,
	const FArcTQSQueryInstance& Query)
{
	using namespace ArcTacticsTracePrivate;

	if (!Report.bAccepted)
	{
		return;
	}
	const FString Source = SourceName.IsNone() ? FString() : SourceName.ToString();
	const FString QueryName = Report.Definition ? Report.Definition->GetName() : FString();
	const TArray<FArcTQSTargetItem>& Items = Query.Items;
	const int32 ItemCount = Items.Num();
	const TArray<FArcTQSStepResult>& Breakdown = Query.StepResults;
	const int32 StepCount = Breakdown.Num();

	TArray<float> QuerierLocation;
	AppendLocation(QuerierLocation, Query.QueryContext.QuerierLocation);

	TStringBuilder<256> StepNames;
	TArray<uint8> StepTypes;
	TArray<int32> StepFilteredCounts;
	StepTypes.Reserve(StepCount);
	StepFilteredCounts.Reserve(StepCount);
	for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
	{
		if (StepIndex > 0)
		{
			StepNames.AppendChar(TEXT('\n'));
		}
		StepNames.Append(Breakdown[StepIndex].StepName);
		StepTypes.Add(static_cast<uint8>(Breakdown[StepIndex].StepType));
		StepFilteredCounts.Add(Breakdown[StepIndex].FilteredCount);
	}

	if (!FitsTraceArray<TCHAR>(Source.Len()) || !FitsTraceArray<TCHAR>(QueryName.Len())
		|| !FitsTraceArray<float>(QuerierLocation.Num())
		|| !FitsTraceArray<TCHAR>(StepNames.Len()) || !FitsTraceArray<uint8>(StepTypes.Num())
		|| !FitsTraceArray<int32>(StepFilteredCounts.Num()))
	{
		return;
	}

	TArray<uint8> Selected;
	TArray<uint8> Applied;
	MarkResults(Query, Report.AppliedCount, Selected, Applied);

	TArray<float> ItemLocations;
	TArray<float> ItemScores;
	TArray<uint8> ItemValid;
	TArray<uint32> ItemEntityIndices;
	TArray<uint32> ItemEntitySerials;
	ItemLocations.Reserve(ItemCount * 3);
	ItemScores.Reserve(ItemCount);
	ItemValid.Reserve(ItemCount);
	ItemEntityIndices.Reserve(ItemCount);
	ItemEntitySerials.Reserve(ItemCount);
	for (const FArcTQSTargetItem& Item : Items)
	{
		AppendLocation(ItemLocations, Item.GetLocation(EntityManager));
		ItemScores.Add(Item.Score);
		ItemValid.Add(Item.bValid ? 1 : 0);
		ItemEntityIndices.Add(static_cast<uint32>(Item.EntityHandle.Index));
		ItemEntitySerials.Add(static_cast<uint32>(Item.EntityHandle.SerialNumber));
	}

	const int64 BytesPerItem = FMath::Max<int64>(3 * sizeof(float), static_cast<int64>(StepCount) * sizeof(float));
	const int64 ItemsPerChunk = FMath::Min<int64>((TraceArrayByteLimit - 1) / BytesPerItem, MAX_int32);
	if (ItemsPerChunk <= 0)
	{
		return;
	}
	const int32 ChunkCount = ItemCount == 0 ? 1 : static_cast<int32>((ItemCount + ItemsPerChunk - 1) / ItemsPerChunk);
	const uint64 WorldId = GetWorldId(World);
	const double WorldTime = GetWorldTime(World);

	TArray<float> StepRawScores;
	TArray<float> StepCumulativeScores;
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
	{
		const int32 ItemOffset = static_cast<int32>(ChunkIndex * ItemsPerChunk);
		const int32 ChunkItems = static_cast<int32>(FMath::Min<int64>(ItemsPerChunk, ItemCount - ItemOffset));
		const bool bFirstChunk = ChunkIndex == 0;

		StepRawScores.Reset(StepCount * ChunkItems);
		StepCumulativeScores.Reset(StepCount * ChunkItems);
		for (const FArcTQSStepResult& Step : Breakdown)
		{
			for (int32 ItemIndex = ItemOffset; ItemIndex < ItemOffset + ChunkItems; ++ItemIndex)
			{
				StepRawScores.Add(Step.RawScores.IsValidIndex(ItemIndex) ? Step.RawScores[ItemIndex] : 0.0f);
				StepCumulativeScores.Add(
					Step.CumulativeScores.IsValidIndex(ItemIndex) ? Step.CumulativeScores[ItemIndex] : 0.0f);
			}
		}

		if (!FitsTraceArray<float>(static_cast<int64>(ChunkItems) * 3) || !FitsTraceArray<float>(ChunkItems)
			|| !FitsTraceArray<uint8>(ChunkItems) || !FitsTraceArray<uint32>(ChunkItems)
			|| !FitsTraceArray<float>(StepRawScores.Num()) || !FitsTraceArray<float>(StepCumulativeScores.Num()))
		{
			return;
		}

		UE_TRACE_LOG(ArcTactics, TQSQuery, ArcTacticsChannel)
			<< TQSQuery.Cycle(FPlatformTime::Cycles64())
			<< TQSQuery.WorldId(WorldId)
			<< TQSQuery.EntityIndex(static_cast<uint32>(Entity.Index))
			<< TQSQuery.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
			<< TQSQuery.WorldTime(WorldTime)
			<< TQSQuery.DecisionGeneration(DecisionGeneration)
			<< TQSQuery.SourceKind(static_cast<uint8>(SourceKind))
			<< TQSQuery.SourceName(*Source, Source.Len())
			<< TQSQuery.ActionSerial(ActionSerial)
			<< TQSQuery.QueryName(*QueryName, QueryName.Len())
			<< TQSQuery.QueryId(Query.QueryId)
			<< TQSQuery.Status(static_cast<uint8>(Query.Status))
			<< TQSQuery.SelectionMode(static_cast<uint8>(Query.SelectionMode))
			<< TQSQuery.QuerierLocation(QuerierLocation.GetData(), bFirstChunk ? QuerierLocation.Num() : 0)
			<< TQSQuery.ChunkIndex(ChunkIndex)
			<< TQSQuery.ChunkCount(ChunkCount)
			<< TQSQuery.ItemOffset(ItemOffset)
			<< TQSQuery.TotalItemCount(ItemCount)
			<< TQSQuery.ItemLocations(ItemLocations.GetData() + static_cast<int64>(ItemOffset) * 3, ChunkItems * 3)
			<< TQSQuery.ItemScores(ItemScores.GetData() + ItemOffset, ChunkItems)
			<< TQSQuery.ItemValid(ItemValid.GetData() + ItemOffset, ChunkItems)
			<< TQSQuery.ItemSelected(Selected.GetData() + ItemOffset, ChunkItems)
			<< TQSQuery.ItemApplied(Applied.GetData() + ItemOffset, ChunkItems)
			<< TQSQuery.ItemEntityIndices(ItemEntityIndices.GetData() + ItemOffset, ChunkItems)
			<< TQSQuery.ItemEntitySerials(ItemEntitySerials.GetData() + ItemOffset, ChunkItems)
			<< TQSQuery.StepNames(StepNames.GetData(), bFirstChunk ? StepNames.Len() : 0)
			<< TQSQuery.StepTypes(StepTypes.GetData(), bFirstChunk ? StepTypes.Num() : 0)
			<< TQSQuery.StepFilteredCounts(StepFilteredCounts.GetData(), bFirstChunk ? StepFilteredCounts.Num() : 0)
			<< TQSQuery.StepRawScores(StepRawScores.GetData(), StepRawScores.Num())
			<< TQSQuery.StepCumulativeScores(StepCumulativeScores.GetData(), StepCumulativeScores.Num());
	}
}

#endif
