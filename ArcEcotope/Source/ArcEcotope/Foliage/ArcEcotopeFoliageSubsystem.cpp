// Copyright Lukasz Baran. All Rights Reserved.

#include "Foliage/ArcEcotopeFoliageSubsystem.h"

#include "ArcEcotopeModule.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainInvokers.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileAccess.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Foliage/ArcEcotopeFoliageAtlas.h"
#include "Foliage/ArcEcotopeFoliageFields.h"
#include "Foliage/ArcEcotopeFoliageWriteShader.h"
#include "Foliage/ArcEcotopeGroundFoliage.h"
#include "GlobalShader.h"
#include "GPUSceneWriter.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Misc/App.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderCompiler.h"
#include "Simulation/ArcForestSimulationState.h"
#include "Simulation/ArcEcotopeVolume.h"

namespace ArcEcotopeFoliageSubsystemPrivate
{
	TAutoConsoleVariable<int32> CVarEnabled(
		TEXT("arcecotope.Foliage.Enabled"),
		1,
		TEXT("Master switch for the ArcEcotope GPU ground foliage layer. 0 tears every patch down."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRegionSizeCm(
		TEXT("arcecotope.Foliage.RegionSizeCm"),
		6400.0f,
		TEXT("Edge, in cm, of the square scatter region centred on the nearest invoker."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarMinGridStepCm(
		TEXT("arcecotope.Foliage.MinGridStepCm"),
		20.0f,
		TEXT("Floor, in cm, on the scatter lattice step derived from density."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarMaxInstancesPerPatch(
		TEXT("arcecotope.Foliage.MaxInstancesPerPatch"),
		131072,
		TEXT("Instance ceiling per patch. Reaching it raises the lattice step instead of truncating the count."),
		ECVF_Default);

	/** Largest lattice cell index either axis may address, keeping the world cell inside the shader's int2. */
	constexpr double MaxLatticeCell = 1.0e9;

#if WITH_EDITOR
	/** Matches the 1200 x 0.25 s cap UArcEntityVisualizationSubsystem waits before abandoning its rebuild. */
	constexpr float MaterialRebuildSettleTimeoutSeconds = 300.0f;
#endif

	struct FArcEcotopeFoliageLattice
	{
		FIntPoint OriginCell = FIntPoint::ZeroValue;
		FIntPoint GridSize = FIntPoint::ZeroValue;
		float GridStepCm = 0.0f;
		FVector2D RegionMin = FVector2D::ZeroVector;
		FVector2D RegionMax = FVector2D::ZeroVector;
		bool bCapRaisedStep = false;
	};

	int64 AxisCellCount(double RegionSizeCm, double StepCm)
	{
		const double Ratio = FMath::CeilToDouble(RegionSizeCm / StepCm);
		return static_cast<int64>(FMath::Clamp(Ratio, 0.0, MaxLatticeCell));
	}

	bool TryComputeLatticeAxis(double NominalMin, double NominalSpanCells, double ClipMin, double ClipMax,
		double StepCm, int32& OutOriginCell, int32& OutCount)
	{
		const double NominalOriginCell = FMath::FloorToDouble(NominalMin / StepCm);

		// Both clip ends ceil, so the cell span is half-open: a cell belongs to the volume containing its
		// anchor and two volumes sharing an edge never address the same cell.
		const double ClipMinCell = FMath::CeilToDouble(ClipMin / StepCm);
		const double ClipMaxCell = FMath::CeilToDouble(ClipMax / StepCm);

		const double StartCell = FMath::Max(NominalOriginCell, ClipMinCell);
		const double EndCell = FMath::Min(NominalOriginCell + NominalSpanCells, ClipMaxCell);
		if (!FMath::IsFinite(StartCell) || !FMath::IsFinite(EndCell)
			|| FMath::Abs(StartCell) > MaxLatticeCell || FMath::Abs(EndCell) > MaxLatticeCell)
		{
			return false;
		}

		const double Count = EndCell - StartCell;
		if (Count <= 0.0)
		{
			OutOriginCell = 0;
			OutCount = 0;
			return true;
		}

		OutOriginCell = static_cast<int32>(StartCell);
		OutCount = static_cast<int32>(Count);
		return true;
	}

	/**
	 * The lattice is the authority on the instance count, not the density number: every random draw is
	 * keyed on the world lattice cell, so a count disagreeing with the lattice would wrap two instances
	 * onto one cell and clone them.
	 *
	 * The nominal square is clipped to the volume rectangle as a half-open cell span, which keeps the
	 * world anchoring exact. A plant's jitter is forward-only, so it can sit up to one lattice step past
	 * the volume's max edge and never below its min edge.
	 */
	bool ComputeLattice(const UArcEcotopeGroundFoliage& Foliage, float DensityScale, const FVector2D& RegionCentre,
		const FBox& VolumeBox, double RegionSizeCm, FArcEcotopeFoliageLattice& OutLattice)
	{
		OutLattice = FArcEcotopeFoliageLattice();

		// ClampMin is an editor-widget clamp that property import bypasses, so the product is clamped here.
		const double DensityProduct = static_cast<double>(Foliage.DensityPerSquareMeter) * static_cast<double>(DensityScale);
		if (!FMath::IsFinite(DensityProduct) || !FMath::IsFinite(RegionSizeCm) || RegionSizeCm <= 0.0)
		{
			return false;
		}

		const double EffectiveDensity = FMath::Max(DensityProduct, static_cast<double>(KINDA_SMALL_NUMBER));
		const double MinGridStepCm = FMath::Max(static_cast<double>(CVarMinGridStepCm.GetValueOnGameThread()), 1.0);
		const int64 MaxInstances = FMath::Max<int64>(CVarMaxInstancesPerPatch.GetValueOnGameThread(), 1);

		double StepCm = FMath::Max(100.0 / FMath::Sqrt(EffectiveDensity), MinGridStepCm);
		int64 AxisCells = AxisCellCount(RegionSizeCm, StepCm);
		while (AxisCells * AxisCells > MaxInstances)
		{
			StepCm *= 1.25;
			AxisCells = AxisCellCount(RegionSizeCm, StepCm);
			OutLattice.bCapRaisedStep = true;
		}

		if (AxisCells <= 0)
		{
			return false;
		}

		const double NominalHalf = static_cast<double>(AxisCells) * StepCm * 0.5;
		int32 OriginX = 0;
		int32 OriginY = 0;
		int32 CountX = 0;
		int32 CountY = 0;
		if (!TryComputeLatticeAxis(RegionCentre.X - NominalHalf, static_cast<double>(AxisCells),
				VolumeBox.Min.X, VolumeBox.Max.X, StepCm, OriginX, CountX)
			|| !TryComputeLatticeAxis(RegionCentre.Y - NominalHalf, static_cast<double>(AxisCells),
				VolumeBox.Min.Y, VolumeBox.Max.Y, StepCm, OriginY, CountY))
		{
			return false;
		}

		if (CountX <= 0 || CountY <= 0)
		{
			return false;
		}

		OutLattice.OriginCell = FIntPoint(OriginX, OriginY);
		OutLattice.GridSize = FIntPoint(CountX, CountY);
		OutLattice.GridStepCm = static_cast<float>(StepCm);
		OutLattice.RegionMin = FVector2D(static_cast<double>(OriginX) * StepCm, static_cast<double>(OriginY) * StepCm);
		OutLattice.RegionMax = OutLattice.RegionMin + FVector2D(static_cast<double>(CountX) * StepCm, static_cast<double>(CountY) * StepCm);
		return true;
	}

	/** Same clamp BuildTerrainData applies: the terrain is an infinite plane and the cell math bounds nothing. */
	bool TryComputeTerrainLocalFilter(const AArcTerrain& Terrain, const FBox& WorldBounds, FBox& OutLocalBounds)
	{
		const FArcTerrainConfig& Config = Terrain.Config;
		if (!WorldBounds.IsValid || !FMath::IsFinite(Config.CellSize()) || Config.CellSize() <= 0.0)
		{
			return false;
		}

		const FVector LocalCornerA = Terrain.WorldToTerrainLocal(WorldBounds.Min);
		const FVector LocalCornerB = Terrain.WorldToTerrainLocal(WorldBounds.Max);
		OutLocalBounds = FBox(LocalCornerA.ComponentMin(LocalCornerB), LocalCornerA.ComponentMax(LocalCornerB));

		const double MaxSpan = static_cast<double>(GArcEcotopeFoliageMaxCells) * Config.CellSize();
		OutLocalBounds.Max.X = FMath::Min(OutLocalBounds.Max.X, OutLocalBounds.Min.X + MaxSpan);
		OutLocalBounds.Max.Y = FMath::Min(OutLocalBounds.Max.Y, OutLocalBounds.Min.Y + MaxSpan);
		return true;
	}

	bool FindNearestInvoker(const FBox& VolumeBox, const TArray<FVector>& Positions, FVector2D& OutCentre)
	{
		double BestDistanceSq = TNumericLimits<double>::Max();
		bool bFound = false;

		for (const FVector& Position : Positions)
		{
			const double DistanceSq = VolumeBox.ComputeSquaredDistanceToPoint(Position);
			if (DistanceSq < BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				OutCentre = FVector2D(Position.X, Position.Y);
				bFound = true;
			}
		}

		return bFound;
	}

#if WITH_EDITOR
	bool IsPlayInEditorSessionActive()
	{
		if (GEngine == nullptr)
		{
			return false;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				return true;
			}
		}

		return false;
	}
#endif
}

bool UArcEcotopeFoliageSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// A dedicated server never loads a baked section, so no patch could ever have a data source.
	return Super::ShouldCreateSubsystem(Outer) && FApp::CanEverRender();
}

bool UArcEcotopeFoliageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UArcEcotopeFoliageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UArcEcotopeFoliageSubsystem::HandleLevelChanged);
	LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UArcEcotopeFoliageSubsystem::HandleLevelChanged);

#if WITH_EDITOR
	if (GEngine != nullptr)
	{
		ActorAddedHandle = GEngine->OnLevelActorAdded().AddUObject(this, &UArcEcotopeFoliageSubsystem::HandleLevelActorChanged);
		ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &UArcEcotopeFoliageSubsystem::HandleLevelActorChanged);
		ActorListChangedHandle = GEngine->OnLevelActorListChanged().AddUObject(this, &UArcEcotopeFoliageSubsystem::HandleLevelActorListChanged);
	}

	MaterialCompilationFinishedHandle = UMaterial::OnMaterialCompilationFinished().AddUObject(
		this, &UArcEcotopeFoliageSubsystem::HandleMaterialCompilationFinished);
#endif
}

void UArcEcotopeFoliageSubsystem::Deinitialize()
{
	// Before World->Scene goes away: every primitive holds a raw FSceneInterface*.
	DestroyAllPatches();
	ReleaseAllFields();

	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
	LevelAddedHandle.Reset();
	LevelRemovedHandle.Reset();

#if WITH_EDITOR
	if (GEngine != nullptr)
	{
		GEngine->OnLevelActorAdded().Remove(ActorAddedHandle);
		GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
		GEngine->OnLevelActorListChanged().Remove(ActorListChangedHandle);
	}
	ActorAddedHandle.Reset();
	ActorDeletedHandle.Reset();
	ActorListChangedHandle.Reset();

	UMaterial::OnMaterialCompilationFinished().Remove(MaterialCompilationFinishedHandle);
	MaterialCompilationFinishedHandle.Reset();
#endif

	CachedVolumes.Reset();
	CachedTerrains.Reset();

	Super::Deinitialize();
}

TStatId UArcEcotopeFoliageSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcEcotopeFoliageSubsystem, STATGROUP_Tickables);
}

void UArcEcotopeFoliageSubsystem::HandleLevelChanged(ULevel* /*Level*/, UWorld* InWorld)
{
	if (InWorld == GetWorld())
	{
		MarkActorCacheDirty();
	}
}

#if WITH_EDITOR
void UArcEcotopeFoliageSubsystem::HandleLevelActorChanged(AActor* Actor)
{
	if (Actor != nullptr && Actor->GetWorld() == GetWorld())
	{
		MarkActorCacheDirty();
	}
}

void UArcEcotopeFoliageSubsystem::HandleLevelActorListChanged()
{
	MarkActorCacheDirty();
}

void UArcEcotopeFoliageSubsystem::HandleMaterialCompilationFinished(UMaterialInterface* /*Material*/)
{
	// A patch keeps no record of which material a slot resolved to, so any completion rebuilds all of them.
	bMaterialRebuildPending = true;
	MaterialRebuildWaitSeconds = 0.0f;
}

void UArcEcotopeFoliageSubsystem::RebuildPatchPrimitives()
{
	for (TUniquePtr<FArcEcotopeFoliagePatch>& Patch : Patches)
	{
		if (Patch->Primitive)
		{
			FArcEcotopeFoliagePrimitive::Release(MoveTemp(Patch->Primitive));
		}

		// A fresh primitive starts with no GPUScene instances, so the patch has to re-dispatch as well.
		Patch->bHasSignature = false;
	}
}
#endif

void UArcEcotopeFoliageSubsystem::MarkActorCacheDirty()
{
	bActorCacheDirty = true;
}

void UArcEcotopeFoliageSubsystem::RefreshActorCache(UWorld& World)
{
	CachedVolumes.Reset();
	for (TActorIterator<AArcEcotopeVolume> VolumeIt(&World); VolumeIt; ++VolumeIt)
	{
		if (IsValid(*VolumeIt))
		{
			CachedVolumes.Emplace(*VolumeIt);
		}
	}

	CachedTerrains.Reset();
	for (TActorIterator<AArcTerrain> TerrainIt(&World); TerrainIt; ++TerrainIt)
	{
		AArcTerrain* Terrain = *TerrainIt;
		if (!IsValid(Terrain))
		{
			continue;
		}

		const FArcTerrainConfig& Config = Terrain->Config;
		if (!FMath::IsFinite(Config.CellSize()) || Config.CellSize() <= 0.0)
		{
			continue;
		}

		CachedTerrains.Emplace(Terrain);
	}

	bActorCacheDirty = false;
}

AArcTerrain* UArcEcotopeFoliageSubsystem::SelectTerrain(const FBox& RegionWorldBounds) const
{
	using namespace ArcEcotopeFoliageSubsystemPrivate;

	AArcTerrain* Selected = nullptr;
	FString SelectedPathName;
	int32 SelectedCellCount = 0;

	TArray<FArcTerrainCellKey> Cells;
	for (const TWeakObjectPtr<AArcTerrain>& TerrainPtr : CachedTerrains)
	{
		AArcTerrain* Terrain = TerrainPtr.Get();
		if (!IsValid(Terrain))
		{
			continue;
		}

		FBox LocalBounds(ForceInit);
		if (!TryComputeTerrainLocalFilter(*Terrain, RegionWorldBounds, LocalBounds))
		{
			continue;
		}

		// Foliage samples the height rail alone, and only from tiles already in memory: a terrain whose
		// tiles have not arrived ranks zero rather than winning the patch and then drawing nothing.
		ArcTerrain::CellsIntersectingLocalBounds(LocalBounds, Terrain->Config, Cells);
		int32 HeightCellCount = 0;
		for (const FArcTerrainCellKey& CellKey : Cells)
		{
			FIntPoint TileKey = FIntPoint::ZeroValue;
			if (ArcTerrainTileAccess::TileKeyFromCell(*Terrain, EArcTerrainRail::Height, 0, CellKey, TileKey)
				&& ArcTerrainTileAccess::FindLoadedTile(*Terrain, EArcTerrainRail::Height, 0, TileKey).IsValid())
			{
				++HeightCellCount;
			}
		}
		if (HeightCellCount == 0)
		{
			continue;
		}

		// Ranked on resident coverage, tie-broken on path name, so two overlapping terrains always
		// resolve to the same one instead of flickering the patch between them.
		const FString PathName = Terrain->GetPathName();
		const bool bBetter = Selected == nullptr
			|| HeightCellCount > SelectedCellCount
			|| (HeightCellCount == SelectedCellCount && PathName.Compare(SelectedPathName, ESearchCase::CaseSensitive) < 0);
		if (bBetter)
		{
			Selected = Terrain;
			SelectedCellCount = HeightCellCount;
			SelectedPathName = PathName;
		}
	}

	return Selected;
}

FArcEcotopeFoliagePatch& UArcEcotopeFoliageSubsystem::FindOrCreatePatch(AArcEcotopeVolume& Volume,
	int32 EntryIndex, int32 MeshIndex, UArcEcotopeGroundFoliage& Foliage)
{
	const FArcEcotopeGroundFoliageMesh& MeshEntry = Foliage.Meshes[MeshIndex];

	TArray<FObjectKey> MaterialKeys;
	MaterialKeys.Reserve(MeshEntry.MaterialOverrides.Num());
	for (const TObjectPtr<UMaterialInterface>& Material : MeshEntry.MaterialOverrides)
	{
		MaterialKeys.Emplace(Material.Get());
	}

	const FObjectKey MeshKey(MeshEntry.Mesh.Get());

	for (TUniquePtr<FArcEcotopeFoliagePatch>& Existing : Patches)
	{
		if (Existing->Volume.Get() != &Volume || Existing->EntryIndex != EntryIndex
			|| Existing->MeshIndex != MeshIndex)
		{
			continue;
		}

		if (Existing->Foliage.Get() == &Foliage && Existing->Mesh == MeshKey
			&& Existing->MaterialOverrides == MaterialKeys
			&& Existing->RenderSettings == Foliage.RenderSettings)
		{
			return *Existing;
		}

		// The proxy's mesh, override materials and render settings are fixed at initialization, so a
		// changed asset is a new patch rather than a re-dispatch.
		if (Existing->Primitive)
		{
			FArcEcotopeFoliagePrimitive::Release(MoveTemp(Existing->Primitive));
		}
		Existing->Foliage = &Foliage;
		Existing->MeshIndex = MeshIndex;
		Existing->Mesh = MeshKey;
		Existing->MaterialOverrides = MoveTemp(MaterialKeys);
		Existing->RenderSettings = Foliage.RenderSettings;
		Existing->bHasSignature = false;
		return *Existing;
	}

	TUniquePtr<FArcEcotopeFoliagePatch> Patch = MakeUnique<FArcEcotopeFoliagePatch>();
	Patch->Volume = &Volume;
	Patch->Foliage = &Foliage;
	Patch->EntryIndex = EntryIndex;
	Patch->MeshIndex = MeshIndex;
	Patch->Mesh = MeshKey;
	Patch->MaterialOverrides = MoveTemp(MaterialKeys);
	Patch->RenderSettings = Foliage.RenderSettings;

	const int32 Index = Patches.Emplace(MoveTemp(Patch));
	return *Patches[Index];
}

FArcEcotopeFoliageFieldRef UArcEcotopeFoliageSubsystem::FindField(AArcEcotopeVolume& Volume)
{
	FArcEcotopeFoliageVolumeField& Entry = VolumeFields.FindOrAdd(FObjectKey(&Volume));
	Entry.Volume = &Volume;

	const FArcEcotopeFoliageFieldPayload& Payload = Volume.FoliageFieldPayload;
	if (!Payload.IsValid())
	{
		ArcEcotopeFoliageFields::ReleaseField(MoveTemp(Entry.Field));
		Entry.bHasField = false;
		if (!Entry.bMissingPayloadLogged)
		{
			UE_LOGF(LogArcEcotope, Warning,"[FoliageFields] volume='%ls' carries no forest field, so its ground foliage will not draw. Commit a simulation to derive one.",
				*Volume.GetName());
			Entry.bMissingPayloadLogged = true;
		}
		return FArcEcotopeFoliageFieldRef();
	}
	Entry.bMissingPayloadLogged = false;

	const FTransform VolumeTransform = Volume.GetActorTransform();
	const bool bSamePayload = Entry.bHasField
		&& Entry.SourceSiteSnapshotHash == Payload.SourceSiteSnapshotHash
		&& Entry.SourceStateChecksum == Payload.SourceStateChecksum;

	if (bSamePayload && Entry.UploadedTransform.Equals(VolumeTransform))
	{
		return Entry.Field;
	}

	if (bSamePayload)
	{
		// Only the placement matrices moved, and the texels are in volume-local space, so the upload
		// is reused rather than repeated.
		FArcEcotopeFoliageFieldRef Rebound =
			ArcEcotopeFoliageFields::MakeFieldWithTransform(Entry.Field, VolumeTransform, NextFieldRevision++);
		if (Rebound.IsValid())
		{
			ArcEcotopeFoliageFields::ReleaseField(MoveTemp(Entry.Field));
			Entry.Field = MoveTemp(Rebound);
			Entry.UploadedTransform = VolumeTransform;
			return Entry.Field;
		}
	}

	ArcEcotopeFoliageFields::ReleaseField(MoveTemp(Entry.Field));
	Entry.Field = ArcEcotopeFoliageFields::MakeFieldFromPayload(Payload, VolumeTransform, NextFieldRevision++);
	Entry.bHasField = Entry.Field.IsValid();
	Entry.SourceSiteSnapshotHash = Payload.SourceSiteSnapshotHash;
	Entry.SourceStateChecksum = Payload.SourceStateChecksum;
	Entry.UploadedTransform = VolumeTransform;
	return Entry.Field;
}

void UArcEcotopeFoliageSubsystem::UpdatePatch(FArcEcotopeFoliagePatch& Patch, AArcEcotopeVolume& Volume,
	const UArcEcotopeGroundFoliage& Foliage, AArcTerrain& Terrain,
	const FIntPoint& RegionOriginCell, const FIntPoint& GridSize, float GridStepCm, const FBox& RegionWorldBounds)
{
	using namespace ArcEcotopeFoliageSubsystemPrivate;

	FArcEcotopeFoliageSignature NewSignature;
	NewSignature.Terrain = FObjectKey(&Terrain);
	NewSignature.RegionOriginCell = RegionOriginCell;
	NewSignature.GridX = GridSize.X;
	NewSignature.GridY = GridSize.Y;
	NewSignature.GridStepCm = GridStepCm;
	// Empty slots count: they are authored holes in the density, and dropping them from the numbering
	// would re-randomise every patch that already exists.
	NewSignature.MeshCount = Foliage.Meshes.Num();
	NewSignature.MinScale = Foliage.MinScale;
	NewSignature.MaxScale = Foliage.MaxScale;
	NewSignature.MaxYawDeg = Foliage.MaxYawDeg;
	NewSignature.MaxSlopeDegrees = Foliage.MaxSlopeDegrees;
	NewSignature.MinLayerWeight = Foliage.MinLayerWeight;
	NewSignature.bAlignToNormal = Foliage.bAlignToNormal;
	NewSignature.TerrainLayerChannel = Foliage.TerrainLayerChannel;
	NewSignature.SnapshotGeneration = Volume.SimulationState != nullptr ? Volume.SimulationState->SnapshotGeneration : 0;
	NewSignature.UnderstoryShade = static_cast<float>(Volume.GetEcotopeOrDefault().UnderstoryShade);
	NewSignature.ShadePreference = Foliage.ShadePreference;
	NewSignature.ShadeStrength = Foliage.ShadeStrength;
	NewSignature.MinMoisture = Foliage.MinMoisture;
	NewSignature.MinFertility = Foliage.MinFertility;

	Patch.Field = FindField(Volume);
	NewSignature.FieldRevision = Patch.Field.IsValid() ? Patch.Field->Revision : 0;

	FBox LocalBounds(ForceInit);
	if (TryComputeTerrainLocalFilter(Terrain, RegionWorldBounds, LocalBounds))
	{
		TArray<FArcTerrainCellKey> Cells;
		ArcTerrain::CellsIntersectingLocalBounds(LocalBounds, Terrain.Config, Cells);

		NewSignature.Cells.Reserve(Cells.Num());
		for (const FArcTerrainCellKey& CellKey : Cells)
		{
			// The signature describes what BuildTerrainData consumes: the identity of the BYTES behind
			// each cell. PayloadId, not the payload's address — a released payload's address is handed
			// straight back to the next one, so a signature keyed on it would miss the swap.
			FIntPoint HeightTileKey = FIntPoint::ZeroValue;
			if (!ArcTerrainTileAccess::TileKeyFromCell(Terrain, EArcTerrainRail::Height, 0, CellKey,
				HeightTileKey))
			{
				continue;
			}
			const FArcTerrainTilePayloadPtr HeightPayload =
				ArcTerrainTileAccess::FindLoadedTile(Terrain, EArcTerrainRail::Height, 0, HeightTileKey);
			if (!HeightPayload.IsValid())
			{
				continue;
			}

			FIntPoint WeightTileKey = FIntPoint::ZeroValue;
			FArcTerrainTilePayloadPtr WeightPayload;
			if (ArcTerrainTileAccess::TileKeyFromCell(Terrain, EArcTerrainRail::Weight, 0, CellKey,
				WeightTileKey))
			{
				WeightPayload =
					ArcTerrainTileAccess::FindLoadedTile(Terrain, EArcTerrainRail::Weight, 0, WeightTileKey);
			}

			FArcEcotopeFoliageCellSignature& CellSignature = NewSignature.Cells.AddDefaulted_GetRef();
			CellSignature.Key = CellKey;
			CellSignature.HeightTileKey = HeightTileKey;
			CellSignature.HeightPayloadId = HeightPayload->PayloadId;
			CellSignature.WeightTileKey = WeightTileKey;
			CellSignature.WeightPayloadId = WeightPayload.IsValid() ? WeightPayload->PayloadId : 0;
		}

		NewSignature.Cells.Sort([](const FArcEcotopeFoliageCellSignature& A, const FArcEcotopeFoliageCellSignature& B)
		{
			return A.Key.Y != B.Key.Y ? A.Key.Y < B.Key.Y : A.Key.X < B.Key.X;
		});
	}

	if (Patch.bHasSignature && Patch.Signature == NewSignature)
	{
		return;
	}

	const FArcEcotopeGroundFoliageMesh& MeshEntry = Foliage.Meshes[Patch.MeshIndex];
	UStaticMesh* Mesh = MeshEntry.Mesh.Get();
	UWorld* World = GetWorld();
	if (Mesh == nullptr || World == nullptr)
	{
		return;
	}

	FArcEcotopeFoliageTerrainData TerrainData;
	if (!ArcEcotopeFoliageAtlas::BuildTerrainData(Terrain, RegionWorldBounds,
		Foliage.TerrainLayerChannel, TerrainData))
	{
		// Nothing sampleable under the region: drop what is drawn rather than leaving a stale dispatch,
		// and record the signature so this is not retried until an input actually changes.
		if (Patch.Primitive)
		{
			Patch.Primitive->RemoveFromScene();
		}
		Patch.Signature = MoveTemp(NewSignature);
		Patch.bHasSignature = true;
		return;
	}

	if (!Patch.Primitive)
	{
		TUniquePtr<FArcEcotopeFoliagePrimitive> Primitive = MakeUnique<FArcEcotopeFoliagePrimitive>();
		if (!Primitive->Initialize(World, &Volume, Mesh, MeshEntry.MaterialOverrides, Patch.RenderSettings))
		{
			return;
		}
		Patch.Primitive = MoveTemp(Primitive);
	}

	const float MinScale = FMath::Max(Foliage.MinScale, 0.01f);
	const float MaxScale = FMath::Max(Foliage.MaxScale, MinScale);

	const FTransform TerrainTransform = Terrain.GetTerrainTransform();
	const double WindowZA =
		TerrainTransform.TransformPosition(FVector(0.0, 0.0, TerrainData.LocalHeightMinCm)).Z;
	const double WindowZB =
		TerrainTransform.TransformPosition(FVector(0.0, 0.0, TerrainData.LocalHeightMaxCm)).Z;

	FBox PatchBounds(
		FVector(RegionWorldBounds.Min.X, RegionWorldBounds.Min.Y, FMath::Min(WindowZA, WindowZB)),
		FVector(RegionWorldBounds.Max.X, RegionWorldBounds.Max.Y, FMath::Max(WindowZA, WindowZB)));
	PatchBounds = PatchBounds.ExpandBy(Mesh->GetBounds().SphereRadius * MaxScale);

	Patch.Primitive->SetTransformAndBounds(FTransform::Identity, FBoxSphereBounds(PatchBounds));

	const int32 NumInstances = GridSize.X * GridSize.Y;
	const int32 NumAllocatedInstances = Patch.Primitive->EnsureInstanceAllocation(NumInstances);
	if (NumAllocatedInstances <= 0 || Patch.Primitive->GetSceneProxy() == nullptr)
	{
		// The mesh's render data may still be compiling; retry on the next tick.
		return;
	}

	const uint32 AlignToNormal = Foliage.bAlignToNormal ? 1u : 0u;
	const float MaxYawRad = FMath::DegreesToRadians(FMath::Clamp(Foliage.MaxYawDeg, 0.0f, 180.0f));
	const float CosMaxSlope = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Foliage.MaxSlopeDegrees, 0.0f, 90.0f)));
	const float MinLayerWeight = FMath::Clamp(Foliage.MinLayerWeight, 0.0f, 1.0f);
	const int32 GridX = GridSize.X;
	const uint32 MeshIndex = static_cast<uint32>(Patch.MeshIndex);
	const uint32 MeshCount = static_cast<uint32>(Foliage.Meshes.Num());

	const float UnderstoryShade = FMath::Max(static_cast<float>(Volume.GetEcotopeOrDefault().UnderstoryShade), 0.0f);
	const float ShadePreference = FMath::Clamp(Foliage.ShadePreference, -1.0f, 1.0f);
	const float ShadeStrength = FMath::Clamp(Foliage.ShadeStrength, 0.0f, 1.0f);
	const float MinMoisture = FMath::Clamp(Foliage.MinMoisture, 0.0f, 1.0f);
	const float MinFertility = FMath::Clamp(Foliage.MinFertility, 0.0f, 1.0f);
	const FArcEcotopeFoliageFieldRef Field = Patch.Field;

	Patch.Primitive->UpdateInstancesGPU(NumInstances,
		[NumInstances, NumAllocatedInstances, GridX, RegionOriginCell, GridStepCm, MinScale, MaxScale, MaxYawRad,
			CosMaxSlope, MinLayerWeight, AlignToNormal, MeshIndex, MeshCount, UnderstoryShade, ShadePreference,
			ShadeStrength, MinMoisture, MinFertility, TerrainData, Field](FRDGBuilder& GraphBuilder, const FGPUSceneWriteDelegateParams& Params)
		{
			if (Params.PersistentPrimitiveId == static_cast<uint32>(INDEX_NONE))
			{
				return;
			}

			FArcEcotopeFoliageScatterCS::FParameters* Parameters =
				GraphBuilder.AllocParameters<FArcEcotopeFoliageScatterCS::FParameters>();
			Parameters->InPrimitiveId = Params.PersistentPrimitiveId;
			Parameters->InNumInstances = static_cast<uint32>(NumInstances);
			Parameters->InNumInstancesAllocated = static_cast<uint32>(NumAllocatedInstances);
			Parameters->InGridX = static_cast<uint32>(GridX);
			Parameters->InPayloadDataFlags = Params.PackedInstanceSceneDataFlags;
			Parameters->InCustomDataCount = Params.NumCustomDataFloats;
			Parameters->InAlignToNormal = AlignToNormal;
			Parameters->InMeshIndex = MeshIndex;
			Parameters->InMeshCount = MeshCount;
			Parameters->InRegionOriginCell = RegionOriginCell;
			Parameters->InGridStepCm = GridStepCm;
			Parameters->InProbeHeightWS = 0.0f;
			Parameters->InMinScale = MinScale;
			Parameters->InMaxScale = MaxScale;
			Parameters->InMaxYawRad = MaxYawRad;
			Parameters->InCosMaxSlope = CosMaxSlope;
			Parameters->InMinLayerWeight = MinLayerWeight;
			Parameters->InUnderstoryShade = UnderstoryShade;
			Parameters->InShadePreference = ShadePreference;
			Parameters->InShadeStrength = ShadeStrength;
			Parameters->InMinMoisture = MinMoisture;
			Parameters->InMinFertility = MinFertility;
			ArcEcotopeFoliageAtlas::BuildTerrainAtlas(GraphBuilder, TerrainData, Parameters->TerrainParameters);
			ArcEcotopeFoliageFields::BindField(GraphBuilder, Field, Parameters->FieldParameters);
			// GPUWriteParams is the deprecated field, not the struct type: the opaque UB replacing it
			// is unusable on DX11 (ENABLE_SCENE_DATA_DX11_UB_ERROR_WORKAROUND).
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Parameters->GPUSceneWriterParameters = Params.GPUWriteParams;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			TShaderMapRef<FArcEcotopeFoliageScatterCS> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));

			// Sized from the ALLOCATED span, not the live count: a shrink leaves the tail slots holding
			// the previous dispatch's transforms and the kernel hides them.
			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCountWrapped(
				FMath::DivideAndRoundUp<int32>(NumAllocatedInstances, FArcEcotopeFoliageScatterCS::NumThreadsPerGroup));

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcEcotopeFoliageScatter"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, Shader, Parameters, GroupCount);
		});

	Patch.Signature = MoveTemp(NewSignature);
	Patch.bHasSignature = true;
}

void UArcEcotopeFoliageSubsystem::DestroyUntouchedPatches()
{
	bool bAnyUntouched = false;
	for (TUniquePtr<FArcEcotopeFoliagePatch>& Patch : Patches)
	{
		if (!Patch->bTouchedThisTick)
		{
			if (Patch->Primitive)
			{
				FArcEcotopeFoliagePrimitive::Release(MoveTemp(Patch->Primitive));
			}
			ArcEcotopeFoliageFields::ReleaseField(MoveTemp(Patch->Field));
			bAnyUntouched = true;
		}
	}

	if (bAnyUntouched)
	{
		Patches.RemoveAll([](const TUniquePtr<FArcEcotopeFoliagePatch>& Patch)
		{
			return !Patch->bTouchedThisTick;
		});
	}
}

void UArcEcotopeFoliageSubsystem::ReleaseFieldsOfDeadVolumes()
{
	// Keyed on the volume's life, not on whether it was reached this tick: an upload is paid once, and
	// a volume the invoker walked away from must not pay it again on the way back.
	TArray<FObjectKey> StaleKeys;
	for (TPair<FObjectKey, FArcEcotopeFoliageVolumeField>& Pair : VolumeFields)
	{
		if (!Pair.Value.Volume.IsValid())
		{
			StaleKeys.Add(Pair.Key);
		}
	}

	for (const FObjectKey& Key : StaleKeys)
	{
		FArcEcotopeFoliageVolumeField Entry;
		VolumeFields.RemoveAndCopyValue(Key, Entry);
		ArcEcotopeFoliageFields::ReleaseField(MoveTemp(Entry.Field));
	}
}

void UArcEcotopeFoliageSubsystem::DestroyAllPatches()
{
	for (TUniquePtr<FArcEcotopeFoliagePatch>& Patch : Patches)
	{
		if (Patch->Primitive)
		{
			FArcEcotopeFoliagePrimitive::Release(MoveTemp(Patch->Primitive));
		}
		ArcEcotopeFoliageFields::ReleaseField(MoveTemp(Patch->Field));
	}

	Patches.Reset();
}

void UArcEcotopeFoliageSubsystem::ReleaseAllFields()
{
	for (TPair<FObjectKey, FArcEcotopeFoliageVolumeField>& Pair : VolumeFields)
	{
		ArcEcotopeFoliageFields::ReleaseField(MoveTemp(Pair.Value.Field));
	}

	VolumeFields.Reset();
}

void UArcEcotopeFoliageSubsystem::Tick(float DeltaTime)
{
	using namespace ArcEcotopeFoliageSubsystemPrivate;

	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (World == nullptr || World->Scene == nullptr)
	{
		return;
	}

	if (CVarEnabled.GetValueOnGameThread() == 0)
	{
		DestroyAllPatches();
		ReleaseAllFields();
		return;
	}

#if WITH_EDITOR
	// Editor and PIE subsystems both tick during a play session; the editor one gives up its GPU budget.
	if (World->WorldType == EWorldType::Editor && IsPlayInEditorSessionActive())
	{
		DestroyAllPatches();
		ReleaseAllFields();
		return;
	}
#endif

	const UArcTerrainSubsystem* TerrainSubsystem = World->GetSubsystem<UArcTerrainSubsystem>();
	if (TerrainSubsystem == nullptr)
	{
		DestroyAllPatches();
		ReleaseAllFields();
		return;
	}

	if (bActorCacheDirty)
	{
		RefreshActorCache(*World);
	}

#if WITH_EDITOR
	if (bMaterialRebuildPending)
	{
		// The delegate fires once per compiled shader-map batch, so consuming it straight away would tear
		// every patch down again on each frame of a compile burst.
		if (GShaderCompilingManager != nullptr && GShaderCompilingManager->IsCompiling())
		{
			MaterialRebuildWaitSeconds += DeltaTime;
			if (MaterialRebuildWaitSeconds >= MaterialRebuildSettleTimeoutSeconds)
			{
				UE_LOGF(LogArcEcotope, Warning,"Shader compilation did not settle within %.0f s; dropping the pending foliage material rebuild.",
					MaterialRebuildSettleTimeoutSeconds);
				bMaterialRebuildPending = false;
				MaterialRebuildWaitSeconds = 0.0f;
			}
		}
		else
		{
			bMaterialRebuildPending = false;
			MaterialRebuildWaitSeconds = 0.0f;
			RebuildPatchPrimitives();
		}
	}
#endif

	int32 NumSubstitutedInvokers = 0;
	ArcTerrain::GatherInvokers(World, InvokerCache, InvokerPositions, NumSubstitutedInvokers);
	if (InvokerPositions.IsEmpty())
	{
		DestroyAllPatches();
		return;
	}

	for (TUniquePtr<FArcEcotopeFoliagePatch>& Patch : Patches)
	{
		Patch->bTouchedThisTick = false;
	}

	const double RegionSizeCm = static_cast<double>(CVarRegionSizeCm.GetValueOnGameThread());

	for (const TWeakObjectPtr<AArcEcotopeVolume>& VolumePtr : CachedVolumes)
	{
		AArcEcotopeVolume* Volume = VolumePtr.Get();
		if (!IsValid(Volume) || Volume->GetEcotopeOrDefault().GroundFoliage.IsEmpty() || Volume->VolumeBounds == nullptr)
		{
			continue;
		}

		// Site.LocalBounds already carries the gradient band, matching what the GPU field is rasterized from.
		const FBox LocalBounds = Volume->GetActiveSiteMetadata().LocalBounds;
		if (!LocalBounds.IsValid)
		{
			continue;
		}

		const FBox VolumeBox = LocalBounds.TransformBy(Volume->GetActorTransform());
		if (!VolumeBox.IsValid)
		{
			continue;
		}

		FVector2D RegionCentre = FVector2D::ZeroVector;
		if (!FindNearestInvoker(VolumeBox, InvokerPositions, RegionCentre))
		{
			continue;
		}

		const TArray<FArcEcotopeGroundFoliageEntry>& GroundFoliage = Volume->GetEcotopeOrDefault().GroundFoliage;
		for (int32 EntryIndex = 0; EntryIndex < GroundFoliage.Num(); ++EntryIndex)
		{
			const FArcEcotopeGroundFoliageEntry& Entry = GroundFoliage[EntryIndex];
			UArcEcotopeGroundFoliage* Foliage = Entry.Foliage.Get();
			if (!Entry.bEnabled || Foliage == nullptr)
			{
				continue;
			}

			FArcEcotopeFoliageLattice Lattice;
			if (!ComputeLattice(*Foliage, Entry.DensityScale, RegionCentre, VolumeBox, RegionSizeCm, Lattice))
			{
				continue;
			}

			const FBox RegionWorldBounds(
				FVector(Lattice.RegionMin.X, Lattice.RegionMin.Y, VolumeBox.Min.Z),
				FVector(Lattice.RegionMax.X, Lattice.RegionMax.Y, VolumeBox.Max.Z));

			AArcTerrain* Terrain = SelectTerrain(RegionWorldBounds);
			if (Terrain == nullptr)
			{
				continue;
			}

			for (int32 MeshIndex = 0; MeshIndex < Foliage->Meshes.Num(); ++MeshIndex)
			{
				if (Foliage->Meshes[MeshIndex].Mesh == nullptr)
				{
					continue;
				}

				FArcEcotopeFoliagePatch& Patch = FindOrCreatePatch(*Volume, EntryIndex, MeshIndex, *Foliage);
				Patch.bTouchedThisTick = true;

				if (MeshIndex == 0)
				{
					if (Lattice.bCapRaisedStep && !Patch.bCapWarningLogged)
					{
						UE_LOGF(LogArcEcotope, Warning,"[Foliage] '%ls' on '%ls' asks for more than arcecotope.Foliage.MaxInstancesPerPatch (%d); "
								 "the lattice step was raised to %.1f cm, which thins the plant below its authored density.",
							*Foliage->GetName(), *Volume->GetName(),
							CVarMaxInstancesPerPatch.GetValueOnGameThread(), Lattice.GridStepCm);
						Patch.bCapWarningLogged = true;
					}
					else if (!Lattice.bCapRaisedStep)
					{
						Patch.bCapWarningLogged = false;
					}
				}

				UpdatePatch(Patch, *Volume, *Foliage, *Terrain, Lattice.OriginCell, Lattice.GridSize,
					Lattice.GridStepCm, RegionWorldBounds);
			}
		}
	}

	DestroyUntouchedPatches();
	ReleaseFieldsOfDeadVolumes();
}
