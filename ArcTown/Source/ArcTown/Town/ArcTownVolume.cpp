// Copyright Lukasz Baran. All Rights Reserved.

#include "Town/ArcTownVolume.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "Catalog/ArcTownBuildingCatalog.h"
#include "Catalog/ArcTownBuildingDefinition.h"
#include "Catalog/ArcTownRoadCatalog.h"
#include "Catalog/ArcTownRoadDefinition.h"
#include "Components/BoxComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Paths/ArcTownPathComponent.h"
#include "Simulation/ArcTownDeterministic.h"
#include "Simulation/ArcTownGrowthEngine.h"
#include "Simulation/ArcTownSimulationState.h"
#include "Simulation/ArcTownTensorField.h"
#include "Town/ArcTownAttractorVolume.h"

#define LOCTEXT_NAMESPACE "ArcTownVolume"

namespace ArcTownVolumeFreeze
{
namespace
{
	constexpr double AxisAlignmentTolerance = 1.0e-4;

	bool SetError(FText& OutError, FText&& Message)
	{
		OutError = MoveTemp(Message);
		return false;
	}

	FBox ComputeBoxTownLocal(const UBoxComponent& Bounds, const FArcTownSiteSnapshot& Site)
	{
		const FTransform ComponentToTerrain = Bounds.GetComponentTransform() * Site.TerrainToWorld.Inverse();
		const FVector Extent = Bounds.GetUnscaledBoxExtent();
		const FBox TerrainLocal = FBox(-Extent, Extent).TransformBy(ComponentToTerrain);

		const FVector2D Origin = ArcTownSiteFrame::TownLocalOriginInTerrainLocal(Site);
		const FVector Shift(Origin.X, Origin.Y, 0.0);
		return FBox(TerrainLocal.Min - Shift, TerrainLocal.Max - Shift);
	}

	FBox2D ToBoundsXY(const FBox& Bounds)
	{
		return FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y));
	}

	bool IsAxisAligned(const FQuat& Rotation)
	{
		const FVector Axes[3] = { Rotation.GetAxisX(), Rotation.GetAxisY(), Rotation.GetAxisZ() };
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			const FVector Magnitudes = Axes[AxisIndex].GetAbs();
			const double Largest = FMath::Max3(Magnitudes.X, Magnitudes.Y, Magnitudes.Z);
			if (Magnitudes.X + Magnitudes.Y + Magnitudes.Z - Largest > AxisAlignmentTolerance)
			{
				return false;
			}
		}
		return true;
	}

	bool ReachesTown(const FBox2D& AttractorBoundsXY, const double InfluenceRadiusCm, const FBox2D& TownBoundsXY)
	{
		if (!FMath::IsFinite(InfluenceRadiusCm) || InfluenceRadiusCm < 0.0)
		{
			return TownBoundsXY.Intersect(AttractorBoundsXY);
		}
		return TownBoundsXY.ExpandBy(InfluenceRadiusCm).Intersect(AttractorBoundsXY);
	}
}
}

AArcTownVolume::AArcTownVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	VolumeBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("VolumeBounds"));
	SetRootComponent(VolumeBounds);
	VolumeBounds->SetBoxExtent(FVector(5000.0, 5000.0, 1000.0));
	VolumeBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VolumeBounds->SetCanEverAffectNavigation(false);
}

void AArcTownVolume::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) && !TownId.IsValid())
	{
		TownId = FGuid::NewGuid();
	}
}

void AArcTownVolume::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject) && !TownId.IsValid())
	{
		TownId = FGuid::NewGuid();
	}
}

void AArcTownVolume::Destroyed()
{
#if WITH_EDITOR
	UWorld* const World = GetWorld();
	const ULevel* const Level = GetLevel();
	// Only a user deleting this volume from an open editor world means "tear the settlement down". A PIE
	// duplicate keeps this TownId, a level being removed destroys its actors wholesale, and an Inactive
	// world (commandlet, asset preview) is not being edited at all — none of those is a delete.
	if (World != nullptr
		&& World->WorldType == EWorldType::Editor
		&& !IsEngineExitRequested()
		&& Level != nullptr
		&& !Level->bIsBeingRemoved
		&& VolumeBounds != nullptr
		&& OnTeardownRequested.IsBound())
	{
		OnTeardownRequested.Execute(*World, this, TownId, VolumeBounds->Bounds.GetBox());
	}
#endif

	Super::Destroyed();
}

bool AArcTownVolume::FreezeSimulationInput(
	const int32 YearsToSimulate, FArcTownSimulationInput& OutInput, FText& OutError) const
{
	using namespace ArcTownVolumeFreeze;

	OutInput = FArcTownSimulationInput();
	OutError = FText::GetEmpty();

	if (!IsInGameThread())
	{
		return SetError(OutError, LOCTEXT("FreezeOffGameThread", "A town simulation input can only be frozen on the game thread."));
	}

	if (YearsToSimulate <= 0)
	{
		return SetError(OutError, LOCTEXT("FreezeYears", "A simulation run needs at least one year to simulate."));
	}

	if (VolumeBounds == nullptr)
	{
		return SetError(OutError, LOCTEXT("FreezeNoBounds", "This town has no volume bounds component."));
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return SetError(OutError, LOCTEXT("FreezeNoWorld", "This town is not in a world."));
	}

	if (!SiteSnapshot.IsValid())
	{
		return SetError(OutError, LOCTEXT("FreezeNoSite", "This town has no usable sampled terrain. Sample Site, then simulate again."));
	}

	if (Catalog == nullptr)
	{
		return SetError(OutError, LOCTEXT("FreezeNoCatalog", "This town has no building catalog. Assign one, then simulate again."));
	}

	if (Catalog->Buildings.Num() == 0)
	{
		return SetError(OutError, FText::Format(
			LOCTEXT("FreezeEmptyCatalog", "Building catalog '{0}' holds no entries, so no town can be built from it."),
			FText::FromString(Catalog->GetName())));
	}

	OutInput.Catalog.Reserve(Catalog->Buildings.Num());
	for (int32 EntryIndex = 0; EntryIndex < Catalog->Buildings.Num(); ++EntryIndex)
	{
		const UArcTownBuildingDefinition* const Building = Catalog->Buildings[EntryIndex];
		if (Building == nullptr)
		{
			// A placed building's CatalogIndex indexes this frozen array, so skipping an empty slot
			// would shift every entry after it onto the wrong building.
			return SetError(OutError, FText::Format(
				LOCTEXT("FreezeEmptySlot", "Building catalog '{0}' entry {1} is empty. Fill the entry, then simulate again."),
				FText::FromString(Catalog->GetName()),
				FText::AsNumber(EntryIndex)));
		}

		const UArcTownBuildingDefinition& Entry = *Building;
		const FArcTownBuildingVisual* const Visual = Entry.Visual.GetPtr();
		if (Visual == nullptr)
		{
			return SetError(OutError, FText::Format(
				LOCTEXT("FreezeEmptyVisual", "Building catalog '{0}' entry {1} names no visual. Fill the entry's Visual, then simulate again."),
				FText::FromString(Catalog->GetName()),
				FText::AsNumber(EntryIndex)));
		}

		const FSoftObjectPath AuthoredVisualPath = Visual->GetAssetPath();
		// The frozen path feeds the deterministic hash, so it is the resolved one: an authored path
		// that still points through a redirector would hash differently from the asset it reaches.
		const UObject* const LoadedVisual = AuthoredVisualPath.TryLoad();
		if (LoadedVisual == nullptr)
		{
			return SetError(OutError, FText::Format(
				LOCTEXT("FreezeUnloadableVisual", "Building catalog '{0}' entry {1} names visual asset '{2}', which will not load. Fix the entry, then simulate again."),
				FText::FromString(Catalog->GetName()),
				FText::AsNumber(EntryIndex),
				FText::FromString(AuthoredVisualPath.ToString())));
		}

		const FSoftObjectPath VisualPath(LoadedVisual);

		FVector2D PlotSizeMinXY = FVector2D::ZeroVector;
		FVector2D PlotSizeMaxXY = FVector2D::ZeroVector;
		ArcTownBuildingDefinition::ResolvePlotSize(Entry, PlotSizeMinXY, PlotSizeMaxXY);

		FArcTownFrozenBuildingEntry Frozen;
		Frozen.VisualPath = VisualPath;
		Frozen.FootprintXY = Entry.FootprintXY;
		Frozen.PlotSizeMinXY = PlotSizeMinXY;
		Frozen.PlotSizeMaxXY = PlotSizeMaxXY;
		Frozen.FloorHeightAbovePivot = Entry.FloorHeightAbovePivot;
		Frozen.MeshLocalDoorForwardXY = ArcTownBuildingDefinition::ResolveDoorForwardXY(Entry, *Visual);
		Frozen.BuildingTags = Entry.BuildingTags;
		Frozen.InfluenceRadiusCm = Entry.InfluenceRadius;
		Frozen.PreferredDistricts = Entry.PreferredDistricts;
		Frozen.PreferredBuildings = Entry.PreferredBuildings;
		Frozen.DenyBuildings = Entry.DenyBuildings;
		Frozen.DenyMinDistanceCm = Entry.DenyMinDistance;
		Frozen.SelectionWeight = Entry.SelectionWeight;
		Frozen.EarliestYear = Entry.EarliestYear;
		Frozen.MinHierarchy = Entry.MinHierarchy;
		Frozen.MaxHierarchy = Entry.MaxHierarchy;
		OutInput.Catalog.Add(MoveTemp(Frozen));
	}

	const FTransform VolumeToTerrain = VolumeBounds->GetComponentTransform() * SiteSnapshot.TerrainToWorld.Inverse();
	if (!IsAxisAligned(VolumeToTerrain.GetRotation()))
	{
		return SetError(OutError, LOCTEXT("FreezeRotatedVolume", "This town's volume is rotated relative to its terrain, so its bounds would take in ground the authored box excludes. Clear the volume's rotation, then simulate again."));
	}

	OutInput.VolumeBoundsTownLocal = ComputeBoxTownLocal(*VolumeBounds, SiteSnapshot);
	if (!ArcTownSiteFrame::HasUsableExtentXY(OutInput.VolumeBoundsTownLocal))
	{
		return SetError(OutError, LOCTEXT("FreezeDegenerateBounds", "This town's volume bounds are empty, so roads have nowhere to grow."));
	}

	const FBox2D TownBoundsXY = ToBoundsXY(OutInput.VolumeBoundsTownLocal);

	for (TActorIterator<AArcTownAttractorVolume> It(World); It; ++It)
	{
		const AArcTownAttractorVolume* const Attractor = *It;
		if (Attractor == nullptr || Attractor->VolumeBounds == nullptr)
		{
			continue;
		}

		const FBox2D AttractorBoundsXY = ToBoundsXY(ComputeBoxTownLocal(*Attractor->VolumeBounds, SiteSnapshot));
		if (!ReachesTown(AttractorBoundsXY, Attractor->InfluenceRadius, TownBoundsXY))
		{
			continue;
		}

		const FVector CenterWorld = Attractor->VolumeBounds->GetComponentLocation();
		const FVector CenterTerrainLocal = SiteSnapshot.TerrainToWorld.InverseTransformPosition(CenterWorld);
		const FVector2D CenterTownLocal = ArcTownSiteFrame::TerrainLocalToTownLocalXY(
			SiteSnapshot, FVector2D(CenterTerrainLocal.X, CenterTerrainLocal.Y));

		FArcTownFrozenAttractor Frozen;
		Frozen.CenterTownLocalXY = FVector2f(
			static_cast<float>(CenterTownLocal.X), static_cast<float>(CenterTownLocal.Y));
		Frozen.Strength = Attractor->Strength;
		Frozen.InfluenceRadiusCm = Attractor->InfluenceRadius;
		Frozen.DistrictTags = Attractor->DistrictTags;
		Frozen.PathName = Attractor->GetPathName();
		OutInput.Attractors.Add(MoveTemp(Frozen));
	}

	ArcTownGrowthEngine::SortAttractors(OutInput.Attractors);

	OutInput.SimulationId = ArcTownDeterministic::MakeSimulationId(TownId, Seed);
	OutInput.Seed = Seed;
	OutInput.SiteSnapshotHash = SiteSnapshot.SnapshotHash;
	OutInput.RuleVersion = ArcTownDeterministic::SimulationRuleVersion;
	OutInput.Site = SiteSnapshot;
	OutInput.Settings = GrowthSettings;
	OutInput.YearsToSimulate = YearsToSimulate;
	OutInput.SourceSnapshot = SimulationState != nullptr ? SimulationState->Snapshot : FArcTownWorldSnapshot();
	OutInput.SettingsHash = ArcTownGrowthEngine::ComputeSettingsHash(
		OutInput.Settings, OutInput.VolumeBoundsTownLocal, OutInput.Catalog, OutInput.Attractors);

	return true;
}

#if WITH_EDITOR

AArcTownVolume::FOnArcTownTeardownRequested AArcTownVolume::OnTeardownRequested;

namespace ArcTownVolumePrivate
{
	void ResetIdentityForCopy(
		FGuid& TownId,
		TArray<FArcTownEmittedActor>& EmittedActors,
		TArray<FArcTownEmittedActor>& EmittedEntityHosts)
	{
		TownId = FGuid::NewGuid();
		// The copied records name the source town's actors, which the new identity does not own.
		EmittedActors.Empty();
		EmittedEntityHosts.Empty();
	}

	/** The terrain the site was sampled from, or null when it is unloaded or was never sampled. */
	const AArcTerrain* FindSampledTerrain(const UWorld* World, const FGuid& TerrainGuid)
	{
		if (World == nullptr || !TerrainGuid.IsValid())
		{
			return nullptr;
		}
		for (TActorIterator<AArcTerrain> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (IsValid(*It) && It->GetActorGuid() == TerrainGuid)
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool IsNearlyUniform(const FVector& Scale)
	{
		// A scale authored as float widens to a double that misses its siblings by a relative ~1e-7,
		// so the tolerance has to grow with the scale rather than sit at an absolute epsilon.
		const double Tolerance = UE_DOUBLE_KINDA_SMALL_NUMBER * FMath::Max(1.0, FMath::Abs(Scale.X));
		return FMath::IsNearlyEqual(Scale.X, Scale.Y, Tolerance)
			&& FMath::IsNearlyEqual(Scale.X, Scale.Z, Tolerance);
	}
}

void AArcTownVolume::PostDuplicate(const bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// A PIE duplicate must keep TownId: it is the same town, streamed into the play world.
	if (!bDuplicateForPIE && !HasAnyFlags(RF_ClassDefaultObject))
	{
		ArcTownVolumePrivate::ResetIdentityForCopy(TownId, EmittedActors, EmittedEntityHosts);
	}
}

void AArcTownVolume::PostEditImport()
{
	Super::PostEditImport();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ArcTownVolumePrivate::ResetIdentityForCopy(TownId, EmittedActors, EmittedEntityHosts);
	}
}

EDataValidationResult AArcTownVolume::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const TFunction<void(const FString&)> AddError = [&Context, &Result](const FString& Message) -> void
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};
	const TFunction<void(const FString&)> AddWarning = [&Context](const FString& Message) -> void
	{
		Context.AddWarning(FText::FromString(Message));
	};

	if (!TownId.IsValid())
	{
		AddError(TEXT("TownId must be a valid GUID."));
	}

	if (Catalog == nullptr)
	{
		AddError(TEXT("Catalog must be set; a town has nothing to place without one."));
	}
	else if (Catalog->Buildings.Num() == 0)
	{
		AddWarning(TEXT("Catalog holds no entries, so a rebuild places no buildings."));
	}

	const FVector ActorScale = GetActorScale3D();
	if (!FMath::IsFinite(ActorScale.X)
		|| !FMath::IsFinite(ActorScale.Y)
		|| !FMath::IsFinite(ActorScale.Z)
		|| ActorScale.X <= 0.0
		|| ActorScale.Y <= 0.0
		|| ActorScale.Z <= 0.0
		|| !ArcTownVolumePrivate::IsNearlyUniform(ActorScale))
	{
		AddError(TEXT("ArcTown volumes require finite, positive, uniform actor scale."));
	}

	if (VolumeBounds == nullptr)
	{
		AddError(TEXT("VolumeBounds component is missing."));
	}
	else
	{
		const FVector Extent = VolumeBounds->GetUnscaledBoxExtent();
		if (!FMath::IsFinite(Extent.X)
			|| !FMath::IsFinite(Extent.Y)
			|| !FMath::IsFinite(Extent.Z)
			|| Extent.X <= 0.0
			|| Extent.Y <= 0.0
			|| Extent.Z <= 0.0)
		{
			AddError(TEXT("VolumeBounds must have finite, positive extents."));
		}
	}

	if (!FMath::IsFinite(GrowthSettings.Roads.MaxTurnPerStepDeg)
		|| !FMath::IsFinite(GrowthSettings.Roads.ExpansionAngleDeviationDeg)
		|| GrowthSettings.Roads.ExpansionAngleDeviationDeg >= GrowthSettings.Roads.MaxTurnPerStepDeg)
	{
		AddError(FString::Printf(
			TEXT("ExpansionAngleDeviationDeg (%.1f deg) must stay below MaxTurnPerStepDeg (%.1f deg): at or above it the clamp saturates on nearly every step, and the random deviation instead of the tensor field decides which way a road turns."),
			GrowthSettings.Roads.ExpansionAngleDeviationDeg,
			GrowthSettings.Roads.MaxTurnPerStepDeg));
	}

	if (!FMath::IsFinite(PadFalloffBand) || PadFalloffBand < 0.0)
	{
		AddError(TEXT("PadFalloffBand must be finite and non-negative."));
	}

	if (!FMath::IsFinite(PathFalloffCm) || PathFalloffCm < 0.0)
	{
		AddError(TEXT("PathFalloffCm must be finite and non-negative."));
	}

	if (!FMath::IsFinite(PathRaiseCm) || PathRaiseCm < 0.0)
	{
		AddError(TEXT("PathRaiseCm must be finite and non-negative."));
	}

	TArray<UArcTownPathComponent*> Paths;
	GetComponents<UArcTownPathComponent>(Paths);
	if (!Paths.IsEmpty() && RoadCatalog != nullptr)
	{
		TArray<FName> RoadChannels;
		for (const TObjectPtr<UArcTownRoadDefinition>& Definition : RoadCatalog->ClassesByHierarchy)
		{
			if (Definition != nullptr && !Definition->SurfaceChannel.IsNone())
			{
				RoadChannels.AddUnique(Definition->SurfaceChannel);
			}
		}

		if (!RoadChannels.IsEmpty())
		{
			const AArcTerrain* const Terrain =
				ArcTownVolumePrivate::FindSampledTerrain(GetWorld(), SiteSnapshot.TerrainGuid);
			for (const FName& Channel : RoadChannels)
			{
				if (Terrain == nullptr)
				{
					AddWarning(FString::Printf(
						TEXT("Road surface channel '%s' could not be checked: the sampled terrain is not loaded. Sample the site and validate again."),
						*Channel.ToString()));
				}
				// A layer whose channel does not resolve against the terrain's channel set is dropped
				// silently by the composite, so the check has to happen here or not at all.
				else if (Terrain->Config.ChannelSet == nullptr
					|| Terrain->Config.ChannelSet->FindChannelIndex(Channel) == INDEX_NONE)
				{
					AddError(FString::Printf(
						TEXT("Road surface channel '%s' is absent from the channel set of terrain '%s'; the surface layer would be dropped without a word."),
						*Channel.ToString(),
						*Terrain->GetName()));
				}
			}
		}
	}

	if (!FMath::IsFinite(SampleSpacing) || SampleSpacing <= 0.0)
	{
		AddError(TEXT("SampleSpacing must be finite and greater than zero."));
	}

	if (!SiteSnapshot.IsEmpty())
	{
		const int64 SampleCount = SiteSnapshot.GetSampleCount();
		if (SampleCount > FArcTownSiteSnapshot::MaxSampleCount)
		{
			AddError(FString::Printf(
				TEXT("SiteSnapshot grid %dx%d holds %lld samples, above the supported maximum of %lld. Raise SampleSpacing or shrink VolumeBounds."),
				SiteSnapshot.GridDims.X,
				SiteSnapshot.GridDims.Y,
				SampleCount,
				FArcTownSiteSnapshot::MaxSampleCount));
		}
		else if (!FMath::IsFinite(SiteSnapshot.SampleSpacing) || SiteSnapshot.SampleSpacing <= 0.0)
		{
			AddError(FString::Printf(
				TEXT("SiteSnapshot SampleSpacing (%g cm) must be finite and greater than zero. Re-sample the site."),
				SiteSnapshot.SampleSpacing));
		}
		else if (!SiteSnapshot.IsValid())
		{
			AddError(FString::Printf(
				TEXT("SiteSnapshot failed validation: grid %dx%d, %d heights, %d normals, terrain %s. Re-sample the site."),
				SiteSnapshot.GridDims.X,
				SiteSnapshot.GridDims.Y,
				SiteSnapshot.Heights.Num(),
				SiteSnapshot.Normals.Num(),
				*SiteSnapshot.TerrainGuid.ToString()));
		}
	}

	return Result;
}

#endif

#undef LOCTEXT_NAMESPACE
