// Copyright Lukasz Baran. All Rights Reserved.

#include "Details/ArcTownVolumeDetails.h"

#include "ArcTownEditorModule.h"
#include "Build/ArcTownInstanceEmitter.h"
#include "Build/ArcTownPathSync.h"
#include "Build/ArcTownTeardown.h"
#include "Components/BoxComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/World.h"
#include "Layout/ArcTownLayout.h"
#include "ScopedTransaction.h"
#include "Simulation/ArcTownSimulationController.h"
#include "Simulation/ArcTownSimulationState.h"
#include "Site/ArcTownSiteSampler.h"
#include "Town/ArcTownSiteSnapshot.h"
#include "Town/ArcTownVolume.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcTownVolumeDetails"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownRebuild, Log, All);

namespace ArcTownVolumeDetailsPrivate
{
	struct FEditorStatus
	{
		int32 EmittedBuildingCount = INDEX_NONE;
		int32 CellCount = INDEX_NONE;
		int32 RoadSplineCount = INDEX_NONE;
		int32 OrphansSwept = INDEX_NONE;
		FText LastError;
	};

	/** Keyed on TownId so the panel survives reselect without writing anything into the package. */
	TMap<FGuid, FEditorStatus> StatusByTownId;

	const FEditorStatus* FindStatus(const AArcTownVolume* Town)
	{
		return Town != nullptr && Town->TownId.IsValid() ? StatusByTownId.Find(Town->TownId) : nullptr;
	}

	FEditorStatus& FindOrAddStatus(const AArcTownVolume& Town)
	{
		return StatusByTownId.FindOrAdd(Town.TownId);
	}

	const FArcTownWorldSnapshot* FindSimulationSnapshot(const AArcTownVolume* Town)
	{
		return Town != nullptr && Town->SimulationState != nullptr ? &Town->SimulationState->Snapshot : nullptr;
	}

	TOptional<FArcTownSimulationSessionSnapshot> FindSessionSnapshot(
		const FArcTownSimulationController* const Controller,
		const AArcTownVolume* const Town)
	{
		if (Controller == nullptr || Town == nullptr)
		{
			return {};
		}
		return Controller->GetSessionSnapshot(*Town);
	}

	FText SessionStatusToText(const EArcTownSimulationSessionStatus Status)
	{
		switch (Status)
		{
		case EArcTownSimulationSessionStatus::Running:
			return LOCTEXT("SessionRunning", "Running");
		case EArcTownSimulationSessionStatus::Cancelling:
			return LOCTEXT("SessionCancelling", "Cancelling");
		case EArcTownSimulationSessionStatus::Succeeded:
			return LOCTEXT("SessionSucceeded", "Committed");
		case EArcTownSimulationSessionStatus::Cancelled:
			return LOCTEXT("SessionCancelled", "Cancelled, nothing committed");
		case EArcTownSimulationSessionStatus::Stale:
			return LOCTEXT("SessionStale", "Dropped, the town changed");
		default:
			return LOCTEXT("SessionFailed", "Failed");
		}
	}

	FText SessionPhaseToText(const EArcTownSimulationPhase Phase)
	{
		switch (Phase)
		{
		case EArcTownSimulationPhase::Simulating:
			return LOCTEXT("PhaseSimulating", "simulating");
		case EArcTownSimulationPhase::Complete:
			return LOCTEXT("PhaseComplete", "complete");
		case EArcTownSimulationPhase::Cancelled:
			return LOCTEXT("PhaseCancelled", "cancelled");
		case EArcTownSimulationPhase::Failed:
			return LOCTEXT("PhaseFailed", "failed");
		default:
			return LOCTEXT("PhaseIdle", "idle");
		}
	}

	TSharedRef<SWidget> MakeButton(
		const TAttribute<FText>& Label,
		const FText& ToolTip,
		const FOnClicked& OnClicked,
		const TAttribute<bool>& IsEnabled = true)
	{
		return SNew(SBox)
			.MinDesiredWidth(150.0f)
			[
				SNew(SButton)
				.Text(Label)
				.ToolTipText(ToolTip)
				.IsEnabled(IsEnabled)
				.OnClicked(OnClicked)
			];
	}
}

TSharedRef<IDetailCustomization> FArcTownVolumeDetails::MakeInstance()
{
	return MakeShared<FArcTownVolumeDetails>();
}

void FArcTownVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}

	SelectedVolume = Cast<AArcTownVolume>(Objects[0].Get());
	if (!SelectedVolume.IsValid())
	{
		return;
	}

	using namespace ArcTownVolumeDetailsPrivate;
	IDetailCategoryBuilder& ActionsCategory = DetailBuilder.EditCategory(TEXT("ArcTown Actions"));
	ActionsCategory.AddCustomRow(LOCTEXT("SampleSiteFilter", "Sample Site"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("SampleSite", "Sample Site"),
		LOCTEXT("SampleSiteTip", "Freeze the baked ArcTerrain surface under this volume. Every terrain cell the sample grid reaches must be built. It reads the COMPOSITED surface, this town's own roads and pads included, so sampling again after a Rebuild feeds Path Raise Cm back in and the roads climb by it every cycle. Sample before you build, and re-sample only after a Clear."),
		FOnClicked::CreateSP(this, &FArcTownVolumeDetails::SampleSite),
		TAttribute<bool>::CreateSP(this, &FArcTownVolumeDetails::CanStartSimulation))];
	ActionsCategory.AddCustomRow(LOCTEXT("SimulateFilter", "Simulate"))
	.WholeRowContent()[MakeButton(
		TAttribute<FText>::CreateSP(this, &FArcTownVolumeDetails::GetSimulateLabelText),
		LOCTEXT("SimulateTip", "Grow this town for Years Per Run more years on a worker thread, then commit the result. Reset Simulation stops a run, and a run that is stopped commits nothing."),
		FOnClicked::CreateSP(this, &FArcTownVolumeDetails::Simulate),
		TAttribute<bool>::CreateSP(this, &FArcTownVolumeDetails::CanStartSimulation))];
	ActionsCategory.AddCustomRow(LOCTEXT("ResetSimulationFilter", "Reset Simulation"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("ResetSimulation", "Reset Simulation"),
		LOCTEXT("ResetSimulationTip", "Stop any run in flight and throw away this town's simulated roads and buildings, so the next run starts from year zero."),
		FOnClicked::CreateSP(this, &FArcTownVolumeDetails::ResetSimulation),
		TAttribute<bool>::CreateSP(this, &FArcTownVolumeDetails::CanMutateState))];
	ActionsCategory.AddCustomRow(LOCTEXT("RebuildFilter", "Rebuild"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("Rebuild", "Rebuild"),
		LOCTEXT("RebuildTip", "Lay the settlement out again and recreate the instance components of every partition actor this town owns."),
		FOnClicked::CreateSP(this, &FArcTownVolumeDetails::Rebuild),
		TAttribute<bool>::CreateSP(this, &FArcTownVolumeDetails::CanStartSimulation))];
	ActionsCategory.AddCustomRow(LOCTEXT("ClearFilter", "Clear"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("Clear", "Clear"),
		LOCTEXT("ClearTip", "Destroy every partition actor this town owns and empty its record of them. The volume itself is left in place. This cannot be undone: rebuild the town to get its buildings back."),
		FOnClicked::CreateSP(this, &FArcTownVolumeDetails::Clear),
		TAttribute<bool>::CreateSP(this, &FArcTownVolumeDetails::CanStartSimulation))];
	ActionsCategory.AddCustomRow(LOCTEXT("SweepOrphansFilter", "Sweep Orphans"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("SweepOrphans", "Sweep Orphans"),
		LOCTEXT("SweepOrphansTip", "Destroy every ArcTown partition actor in this world whose owning volume no longer exists. Each one is named in the log. This cannot be undone."),
		FOnClicked::CreateSP(this, &FArcTownVolumeDetails::SweepOrphans),
		TAttribute<bool>::CreateSP(this, &FArcTownVolumeDetails::CanStartSimulation))];

	IDetailCategoryBuilder& SimulationCategory = DetailBuilder.EditCategory(TEXT("ArcTown Simulation"));
	SimulationCategory.AddCustomRow(LOCTEXT("SimulationYearFilter", "Year"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("SimulationYearLabel", "Year"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetSimulationYearText)];
	SimulationCategory.AddCustomRow(LOCTEXT("NodeCountFilter", "Nodes"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("NodeCountLabel", "Road nodes"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetNodeCountText)];
	SimulationCategory.AddCustomRow(LOCTEXT("RoadCountFilter", "Roads"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("RoadCountLabel", "Roads built / planned"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetRoadCountText)];
	SimulationCategory.AddCustomRow(LOCTEXT("SimulatedBuildingCountFilter", "Buildings"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("SimulatedBuildingCountLabel", "Buildings"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetSimulatedBuildingCountText)];
	SimulationCategory.AddCustomRow(LOCTEXT("UnreachableTripsFilter", "Unreachable Trips"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("UnreachableTripsLabel", "Trips without a reachable target"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetUnreachableTripCountText)];
	SimulationCategory.AddCustomRow(LOCTEXT("ProgressFilter", "Progress"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("ProgressLabel", "Progress"))]
	.ValueContent()[SNew(STextBlock).AutoWrapText(true).Text(this, &FArcTownVolumeDetails::GetProgressText)];

	IDetailCategoryBuilder& StatusCategory = DetailBuilder.EditCategory(TEXT("ArcTown Status"));
	StatusCategory.AddCustomRow(LOCTEXT("SiteStatusFilter", "Site"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("SiteStatusLabel", "Site"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetSiteStatusText)];
	StatusCategory.AddCustomRow(LOCTEXT("EmittedBuildingCountFilter", "Emitted Buildings"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("EmittedBuildingCountLabel", "Emitted buildings"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetEmittedBuildingCountText)];
	StatusCategory.AddCustomRow(LOCTEXT("CellCountFilter", "Cells"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("CellCountLabel", "Cells"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetCellCountText)];
	StatusCategory.AddCustomRow(LOCTEXT("RoadSplineCountFilter", "Road Splines"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("RoadSplineCountLabel", "Road splines"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetRoadSplineCountText)];
	StatusCategory.AddCustomRow(LOCTEXT("OrphansSweptFilter", "Orphans Swept"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("OrphansSweptLabel", "Orphans swept"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcTownVolumeDetails::GetOrphansSweptText)];
	StatusCategory.AddCustomRow(LOCTEXT("LastErrorFilter", "Last Error"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("LastErrorLabel", "Last error"))]
	.ValueContent()[SNew(STextBlock).AutoWrapText(true).Text(this, &FArcTownVolumeDetails::GetLastErrorText)];
}

AArcTownVolume* FArcTownVolumeDetails::GetVolume() const
{
	return SelectedVolume.Get();
}

FArcTownSimulationController* FArcTownVolumeDetails::GetController() const
{
	TSharedPtr<FArcTownSimulationController> Pinned = CachedController.Pin();
	if (!Pinned.IsValid())
	{
		CachedController = FArcTownEditorModule::FindSimulationController();
		Pinned = CachedController.Pin();
	}
	return Pinned.Get();
}

bool FArcTownVolumeDetails::CanMutateState() const
{
	const AArcTownVolume* const Town = GetVolume();
	const UWorld* const World = Town != nullptr ? Town->GetWorld() : nullptr;
	return World != nullptr && !World->IsGameWorld();
}

bool FArcTownVolumeDetails::CanStartSimulation() const
{
	const FArcTownSimulationController* const Controller = GetController();
	return CanMutateState() && Controller != nullptr && !Controller->HasActiveSession();
}

UArcTownSimulationState* FArcTownVolumeDetails::EnsureSimulationState(AArcTownVolume& Town) const
{
	if (Town.SimulationState != nullptr)
	{
		return Town.SimulationState;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateSimulationStateTransaction", "Create ArcTown Simulation State"));
	Town.Modify();
	Town.SimulationState = NewObject<UArcTownSimulationState>(&Town, NAME_None, RF_Transactional);
	return Town.SimulationState;
}

FReply FArcTownVolumeDetails::SampleSite()
{
	AArcTownVolume* const Town = GetVolume();
	if (Town == nullptr || !CanStartSimulation())
	{
		return FReply::Handled();
	}

	FArcTownSiteSnapshot Sampled;
	FText Error;
	if (!ArcTownSiteSampler::SampleSite(*Town, Sampled, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("SampleSiteTransaction", "Sample ArcTown Site"));
	Town->Modify();
	Town->SiteSnapshot = MoveTemp(Sampled);
	ClearLastError();
	return FReply::Handled();
}

FReply FArcTownVolumeDetails::Simulate()
{
	AArcTownVolume* const Town = GetVolume();
	if (Town == nullptr || !CanStartSimulation())
	{
		return FReply::Handled();
	}

	FArcTownSimulationController* const Controller = GetController();
	if (Controller == nullptr)
	{
		SetLastError(LOCTEXT("NoController", "The ArcTown editor module holds no simulation controller."));
		return FReply::Handled();
	}

	EnsureSimulationState(*Town);

	FText Error;
	if (!Controller->Start(*Town, Town->YearsPerRun, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}

	ClearLastError();
	return FReply::Handled();
}

FReply FArcTownVolumeDetails::ResetSimulation()
{
	AArcTownVolume* const Town = GetVolume();
	if (Town == nullptr || !CanMutateState())
	{
		return FReply::Handled();
	}

	if (FArcTownSimulationController* const Controller = GetController())
	{
		Controller->Cancel(*Town);
	}

	const FScopedTransaction Transaction(LOCTEXT("ResetSimulationTransaction", "Reset ArcTown Simulation"));
	Town->Modify();
	if (Town->SimulationState == nullptr)
	{
		Town->SimulationState = NewObject<UArcTownSimulationState>(Town, NAME_None, RF_Transactional);
	}
	else
	{
		Town->SimulationState->Modify();
		Town->SimulationState->Snapshot = FArcTownWorldSnapshot();
	}
	ClearLastError();
	return FReply::Handled();
}

FReply FArcTownVolumeDetails::Rebuild()
{
	AArcTownVolume* const Town = GetVolume();
	if (Town == nullptr || !CanStartSimulation())
	{
		return FReply::Handled();
	}

	TArray<FArcTownGeneratedPath> LayoutPaths;
	TArray<FArcTownGeneratedJunction> LayoutJunctions;
	FText Error;
	const double LayoutStartSeconds = FPlatformTime::Seconds();
	if (!FArcTownInstanceEmitter::ResolveGeneratedPaths(*Town, LayoutPaths, LayoutJunctions, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	const double LayoutSeconds = FPlatformTime::Seconds() - LayoutStartSeconds;

	const FScopedTransaction Transaction(LOCTEXT("RebuildTransaction", "Rebuild ArcTown Instances"));
	const double SyncStartSeconds = FPlatformTime::Seconds();
	ArcTownPathSync::SyncGeneratedPaths(*Town, LayoutPaths);
	const double SyncSeconds = FPlatformTime::Seconds() - SyncStartSeconds;

	FArcTownPreparedInstances Prepared;
	Prepared.Junctions = MoveTemp(LayoutJunctions);
	if (!FArcTownInstanceEmitter::Prepare(*Town, Prepared, Error))
	{
		FArcTownInstanceEmitter::Discard(Prepared);
		SetLastError(Error);
		return FReply::Handled();
	}

	const int32 BuildingCount = Prepared.BuildingCount;
	const int32 CellCount = Prepared.CellCount;

	if (!FArcTownInstanceEmitter::Adopt(*Town, Prepared, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	FArcTownInstanceEmitter::CommitEmittedActorChanges(*Town, Prepared);
	const int32 RoadSplineCount = Prepared.RoadSplineCount;

	UE_LOGF(LogArcTownRebuild, Log,"ArcTown rebuilt '%ls' in %.1f ms (%.1f ms road planarization, %.1f ms path component sync): %d building(s), %d cell(s).",
		*Town->GetName(),
		(FPlatformTime::Seconds() - LayoutStartSeconds) * 1000.0,
		LayoutSeconds * 1000.0,
		SyncSeconds * 1000.0,
		BuildingCount,
		CellCount);

	using namespace ArcTownVolumeDetailsPrivate;
	FEditorStatus& Status = FindOrAddStatus(*Town);
	Status.EmittedBuildingCount = BuildingCount;
	Status.CellCount = CellCount;
	Status.RoadSplineCount = RoadSplineCount;
	Status.LastError = FText::GetEmpty();
	return FReply::Handled();
}

FReply FArcTownVolumeDetails::Clear()
{
	AArcTownVolume* const Town = GetVolume();
	if (Town == nullptr || Town->VolumeBounds == nullptr || !CanStartSimulation())
	{
		return FReply::Handled();
	}

	const int32 RecordedCellCount = Town->EmittedActors.Num();
	const int32 ClearedCount = ArcTownTeardown::ClearTown(
		*Town->GetWorld(), Town, Town->TownId, Town->VolumeBounds->Bounds.GetBox());

	using namespace ArcTownVolumeDetailsPrivate;
	FEditorStatus& Status = FindOrAddStatus(*Town);
	// Measured, not assumed: the teardown can refuse, and it counts partition actors, never buildings.
	Status.CellCount = Town->EmittedActors.Num();
	Status.EmittedBuildingCount = INDEX_NONE;
	Status.RoadSplineCount = INDEX_NONE;
	Status.LastError = ClearedCount == 0 && RecordedCellCount > 0
		? LOCTEXT("ClearDestroyedNothing", "Clear destroyed no partition actor; see the log for why.")
		: FText::GetEmpty();
	return FReply::Handled();
}

FReply FArcTownVolumeDetails::SweepOrphans()
{
	AArcTownVolume* const Town = GetVolume();
	if (Town == nullptr || !CanStartSimulation())
	{
		return FReply::Handled();
	}

	using namespace ArcTownVolumeDetailsPrivate;
	FindOrAddStatus(*Town).OrphansSwept = ArcTownTeardown::SweepOrphans(*Town->GetWorld());
	return FReply::Handled();
}

FText FArcTownVolumeDetails::GetSimulateLabelText() const
{
	const AArcTownVolume* const Town = GetVolume();
	return FText::Format(
		LOCTEXT("SimulateFormat", "Simulate {0} Years"),
		FText::AsNumber(Town != nullptr ? Town->YearsPerRun : 0));
}

FText FArcTownVolumeDetails::GetSiteStatusText() const
{
	const AArcTownVolume* const Town = GetVolume();
	if (Town == nullptr)
	{
		return FText::GetEmpty();
	}
	if (Town->SiteSnapshot.IsEmpty())
	{
		return LOCTEXT("SiteNotSampled", "Not sampled");
	}
	// Slate polls this every frame and FArcTownSiteSnapshot::IsValid scans every sample, so the row
	// reports the lattice instead of validating it.
	return FText::FromString(FString::Printf(
		TEXT("%d x %d samples at %.0f cm"),
		Town->SiteSnapshot.GridDims.X,
		Town->SiteSnapshot.GridDims.Y,
		Town->SiteSnapshot.SampleSpacing));
}

FText FArcTownVolumeDetails::GetSimulationYearText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const FArcTownWorldSnapshot* const Snapshot = FindSimulationSnapshot(GetVolume());
	return Snapshot != nullptr && Snapshot->SimulationId.IsValid()
		? FText::AsNumber(Snapshot->Year)
		: LOCTEXT("NotSimulated", "Not simulated");
}

FText FArcTownVolumeDetails::GetNodeCountText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const FArcTownWorldSnapshot* const Snapshot = FindSimulationSnapshot(GetVolume());
	return Snapshot != nullptr ? FText::AsNumber(Snapshot->Nodes.Num()) : LOCTEXT("NotSimulated", "Not simulated");
}

FText FArcTownVolumeDetails::GetRoadCountText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const FArcTownWorldSnapshot* const Snapshot = FindSimulationSnapshot(GetVolume());
	if (Snapshot == nullptr)
	{
		return LOCTEXT("NotSimulated", "Not simulated");
	}

	int32 BuiltCount = 0;
	for (const FArcTownRoadEdge& Edge : Snapshot->Edges)
	{
		BuiltCount += Edge.bBuilt != 0 ? 1 : 0;
	}
	return FText::Format(
		LOCTEXT("RoadCountFormat", "{0} / {1}"),
		FText::AsNumber(BuiltCount),
		FText::AsNumber(Snapshot->Edges.Num()));
}

FText FArcTownVolumeDetails::GetSimulatedBuildingCountText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const FArcTownWorldSnapshot* const Snapshot = FindSimulationSnapshot(GetVolume());
	return Snapshot != nullptr ? FText::AsNumber(Snapshot->Buildings.Num()) : LOCTEXT("NotSimulated", "Not simulated");
}

FText FArcTownVolumeDetails::GetUnreachableTripCountText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const TOptional<FArcTownSimulationSessionSnapshot> Session = FindSessionSnapshot(GetController(), GetVolume());
	if (!Session.IsSet() || Session->Status != EArcTownSimulationSessionStatus::Succeeded)
	{
		return LOCTEXT("NoRunThisSession", "No run this session");
	}
	return FText::Format(
		LOCTEXT("UnreachableTripsFormat", "{0} of {1} trips"),
		FText::AsNumber(Session->FinalYearTraffic.TripsWithoutReachableTarget),
		FText::AsNumber(Session->FinalYearTraffic.TripsRequested));
}

FText FArcTownVolumeDetails::GetProgressText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const TOptional<FArcTownSimulationSessionSnapshot> Session = FindSessionSnapshot(GetController(), GetVolume());
	if (!Session.IsSet())
	{
		return LOCTEXT("NoRunThisSession", "No run this session");
	}

	return FText::FromString(FString::Printf(
		TEXT("%s \u2014 %s (%d/%d years)"),
		*SessionStatusToText(Session->Status).ToString(),
		*SessionPhaseToText(Session->Phase).ToString(),
		Session->CompletedYears,
		Session->TotalYears));
}

FText FArcTownVolumeDetails::GetEmittedBuildingCountText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const FEditorStatus* const Status = FindStatus(GetVolume());
	return Status != nullptr && Status->EmittedBuildingCount != INDEX_NONE
		? FText::AsNumber(Status->EmittedBuildingCount)
		: LOCTEXT("NotBuiltThisSession", "Not built this session");
}

FText FArcTownVolumeDetails::GetCellCountText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const FEditorStatus* const Status = FindStatus(GetVolume());
	return Status != nullptr && Status->CellCount != INDEX_NONE
		? FText::AsNumber(Status->CellCount)
		: LOCTEXT("NotBuiltThisSession", "Not built this session");
}

FText FArcTownVolumeDetails::GetRoadSplineCountText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const FEditorStatus* const Status = FindStatus(GetVolume());
	return Status != nullptr && Status->RoadSplineCount != INDEX_NONE
		? FText::AsNumber(Status->RoadSplineCount)
		: LOCTEXT("NotBuiltThisSession", "Not built this session");
}

FText FArcTownVolumeDetails::GetOrphansSweptText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const FEditorStatus* const Status = FindStatus(GetVolume());
	return Status != nullptr && Status->OrphansSwept != INDEX_NONE
		? FText::AsNumber(Status->OrphansSwept)
		: LOCTEXT("NotSweptThisSession", "Not swept this session");
}

FText FArcTownVolumeDetails::GetLastErrorText() const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const FEditorStatus* const Status = FindStatus(GetVolume());
	if (Status != nullptr && !Status->LastError.IsEmpty())
	{
		return Status->LastError;
	}

	const TOptional<FArcTownSimulationSessionSnapshot> Session = FindSessionSnapshot(GetController(), GetVolume());
	return Session.IsSet() && !Session->Error.IsEmpty() ? FText::FromString(Session->Error) : FText::GetEmpty();
}

void FArcTownVolumeDetails::SetLastError(const FText& Error) const
{
	using namespace ArcTownVolumeDetailsPrivate;
	const AArcTownVolume* const Town = GetVolume();
	if (Town != nullptr)
	{
		FindOrAddStatus(*Town).LastError = Error;
	}
	if (!Error.IsEmpty())
	{
		UE_LOGF(LogArcTownRebuild, Error,"%ls", *Error.ToString());
	}
}

void FArcTownVolumeDetails::ClearLastError() const
{
	SetLastError(FText::GetEmpty());
}

#undef LOCTEXT_NAMESPACE
