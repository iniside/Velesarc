// Copyright Lukasz Baran. All Rights Reserved.

#include "Terrain/PCGGetTerrainSettings.h"

#include "Terrain/PCGTerrainData.h"

#include "PCGCommon.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Helpers/PCGHelpers.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTypes.h"
#include "Surface/ArcTerrainSurfaceSample.h"

#include "Hash/Blake3.h"

#define LOCTEXT_NAMESPACE "PCGGetTerrainElement"

namespace ArcPCGGetTerrainSettingsInternal
{
	static TAutoConsoleVariable<int32> CVarMaxCells(
		TEXT("arc.PCG.Terrain.MaxCells"),
		256,
		TEXT("Max ArcTerrain cells a PCG Get-Terrain node will read baked height tiles from before aborting."));

	/** Marks an unreadable cell in the content hash: a hole and flat ground are different surfaces, so
	 *  the two must not fold identically. */
	constexpr uint8 UnreadableCellHashTag = 0xFFu;

	void AccumulateCellLocalBounds(FBox& InOutLocalBounds, int32 CellX, int32 CellY, double CellSize,
		float MinHeightCm, float MaxHeightCm)
	{
		const double X0 = static_cast<double>(CellX) * CellSize;
		const double X1 = static_cast<double>(CellX + 1) * CellSize;
		const double Y0 = static_cast<double>(CellY) * CellSize;
		const double Y1 = static_cast<double>(CellY + 1) * CellSize;
		InOutLocalBounds += FBox(
			FVector(X0, Y0, static_cast<double>(MinHeightCm)),
			FVector(X1, Y1, static_cast<double>(MaxHeightCm)));
	}
}

UPCGGetTerrainSettings::UPCGGetTerrainSettings()
{
	ActorSelector.bIncludeChildren = false;
	ActorSelector.bShowActorSelectionClass = false;
	ActorSelector.bSelectMultiple = true;
	ActorSelector.bShowSelectMultiple = false;

	// Apply the self-resolving defaults only to a newly placed node (not the CDO/archetype or a
	// loaded instance whose selection the user may have changed) — mirrors UPCGGetLandscapeSettings.
	if (PCGHelpers::IsNewObjectAndNotDefault(this))
	{
		ActorSelector.ActorFilter = EPCGActorFilter::AllWorldActors;
		ActorSelector.bMustOverlapSelf = true;
		ActorSelector.ActorSelection = EPCGActorSelection::ByClass;
		ActorSelector.ActorSelectionClass = AArcTerrain::StaticClass();
	}
}

EPCGDataType UPCGGetTerrainSettings::GetDataFilter() const
{
	return EPCGDataType::Surface;
}

TSubclassOf<AActor> UPCGGetTerrainSettings::GetDefaultActorSelectorClass() const
{
	return AArcTerrain::StaticClass();
}

TArray<FPCGPinProperties> UPCGGetTerrainSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Surface, /*bAllowMultipleConnections=*/true, /*bAllowMultipleData=*/false);

	return PinProperties;
}

#if WITH_EDITOR
FText UPCGGetTerrainSettings::GetNodeTooltipText() const
{
	return LOCTEXT("GetTerrainTooltip", "Builds surface data (height and normal) from the selected ArcTerrain actor's BAKED per-cell height tiles, so it behaves identically in editor, PIE and cooked. A cell with no built section is flat ground at the terrain's Base Height; only a cell whose registered section is unreadable is a hole. No material layer weights: those are sampled GPU-side by a separate node.");
}
#endif

FPCGElementPtr UPCGGetTerrainSettings::CreateElement() const
{
	return MakeShared<FPCGGetTerrainElement>();
}

void FPCGGetTerrainElement::ProcessActors(FPCGContext* Context, const UPCGDataFromActorSettings* InSettings, const TArray<AActor*>& FoundActors, TArray<FPCGTaskId>& OutDynamicDependencies) const
{
	// LoadSectionSync and the UObject reads below are game-thread-only; the base
	// FPCGDataFromActorElement inherits CanExecuteOnlyOnMainThread()==true, so this must hold.
	check(IsInGameThread());
	check(Context);
	check(InSettings);

	const UPCGGetTerrainSettings* Settings = CastChecked<UPCGGetTerrainSettings>(InSettings);

	// bAllowMultipleData=false → one output. Take the first valid AArcTerrain; warn on extras.
	AArcTerrain* Terrain = nullptr;
	int32 NumTerrains = 0;
	for (AActor* FoundActor : FoundActors)
	{
		AArcTerrain* Candidate = Cast<AArcTerrain>(FoundActor);
		if (IsValid(Candidate))
		{
			++NumTerrains;
			if (!Terrain)
			{
				Terrain = Candidate;
			}
		}
	}

	if (!Terrain)
	{
		// No ArcTerrain selected: no output (not an error — matches the landscape getter's empty path).
		return;
	}

	if (NumTerrains > 1)
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("MultipleTerrains", "{0} ArcTerrain actors selected but only one surface is produced; using the first."), NumTerrains));
	}

	const FArcTerrainConfig& Config = Terrain->Config;

	// Restrict the read to the cells intersecting the PCG execution bounds (the PCG volume /
	// component / Bounding Shape). The terrain is potentially unbounded, so we NEVER read the whole
	// thing — we sample only what this PCG graph actually covers, exactly as the landscape node scopes
	// its work via Context->ExecutionSource bounds.
	if (!Context->ExecutionSource.IsValid())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoExecutionBounds", "No PCG execution source; cannot scope the ArcTerrain surface read to a region."));
		return;
	}
	const FBox WorldQueryBounds = Context->ExecutionSource->GetExecutionState().GetBounds();
	if (!WorldQueryBounds.IsValid)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoValidBounds", "PCG execution bounds are invalid; place the graph within a PCG volume or connect a Bounding Shape so the ArcTerrain surface read has a finite region."));
		return;
	}

	// World AABB -> terrain-local (conservative AABB of the transformed corners under any terrain
	// rotation/scale), then to the inclusive cell range covering it. The terrain is an infinite plane,
	// so the region is exactly the query's cell range — there is no fixed terrain rectangle to clip to.
	const FBox LocalQueryBounds = WorldQueryBounds.InverseTransformBy(Terrain->GetTerrainTransform());
	const int32 QueryMinCellX = FMath::FloorToInt(LocalQueryBounds.Min.X / Config.CellSize());
	const int32 QueryMaxCellX = FMath::FloorToInt(LocalQueryBounds.Max.X / Config.CellSize());
	const int32 QueryMinCellY = FMath::FloorToInt(LocalQueryBounds.Min.Y / Config.CellSize());
	const int32 QueryMaxCellY = FMath::FloorToInt(LocalQueryBounds.Max.Y / Config.CellSize());

	FIntRect IterRect; // [Min, Max) half-open — the cells covered by the PCG query bounds
	IterRect.Min.X = QueryMinCellX;
	IterRect.Min.Y = QueryMinCellY;
	IterRect.Max.X = QueryMaxCellX + 1;
	IterRect.Max.Y = QueryMaxCellY + 1;
	if (!(IterRect.Min.X < IterRect.Max.X && IterRect.Min.Y < IterRect.Max.Y))
	{
		// The PCG volume does not overlap the terrain — nothing to sample.
		return;
	}

	const int64 NumCells = static_cast<int64>(IterRect.Max.X - IterRect.Min.X) * static_cast<int64>(IterRect.Max.Y - IterRect.Min.Y);
	const int32 MaxCells = ArcPCGGetTerrainSettingsInternal::CVarMaxCells.GetValueOnGameThread();
	if (NumCells > static_cast<int64>(MaxCells))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("TooManyCells", "The PCG volume covers {0} ArcTerrain cells, exceeding the cap of {1}; shrink the PCG volume or raise arc.PCG.Terrain.MaxCells."), NumCells, MaxCells));
		return;
	}

	TSharedPtr<FArcTerrainSurfaceCache> Cache = MakeShared<FArcTerrainSurfaceCache>();
	Cache->CellSize = Config.CellSize();
	Cache->QuadsPerCell = Config.QuadsPerCell();
	// The cache's own extent is the region actually read (IterRect), not any terrain-wide rectangle —
	// GetLocalExtentCm / ProjectPoint scope to exactly the cells this cache holds.
	Cache->CellBounds = IterRect;
	Cache->TerrainTransform = Terrain->GetTerrainTransform();

	// ContentHash folds the geometry (so a config change invalidates) plus every cell's BAKED height
	// payload — key, tile resolution, quantization window and the raw texel bytes — so any re-bake of
	// a covered cell invalidates. Consumed by UPCGTerrainData::AddToCrc. BaseHeightCm rides here because
	// it IS the surface of every unshaped cell. The composite-input key is deliberately NOT used: it is
	// editor-only state and does not exist in a cooked build.
	FBlake3 Hasher;
	const double CellSizeForHash = Config.CellSize();
	const int32 QuadsForHash = Config.QuadsPerCell();
	const float BaseHeightCm = Terrain->HeightOptions.BaseHeightCm;
	Hasher.Update(&CellSizeForHash, sizeof(CellSizeForHash));
	Hasher.Update(&QuadsForHash, sizeof(QuadsForHash));
	Hasher.Update(&BaseHeightCm, sizeof(BaseHeightCm));
	Hasher.Update(&IterRect, sizeof(IterRect));

	const int32 FlatRenderQuads = Config.QuadsPerCell();
	int32 FlatCellIndex = INDEX_NONE;

	FBox LocalBounds(ForceInit);
	bool bHaveLocalBounds = false;
	int32 CellsRead = 0;
	int32 CellsFlat = 0;
	int32 CellsSkipped = 0;

	// [Min, Max) on both axes — the PCG-bounds-clipped IterRect (max-EXCLUSIVE), NOT the whole terrain.
	for (int32 Y = IterRect.Min.Y; Y < IterRect.Max.Y; ++Y)
	{
		for (int32 X = IterRect.Min.X; X < IterRect.Max.X; ++X)
		{
			const FArcTerrainCellKey Key(X, Y);

			// No tile => nothing shaped this cell, and unshaped ground is flat at BaseHeightCm. Every such
			// key shares one synthetic entry.
			FIntPoint TileKey = FIntPoint::ZeroValue;
			const bool bHasTile =
				ArcTerrainTileAccess::TileKeyFromCell(*Terrain, EArcTerrainRail::Height, 0, Key, TileKey)
				&& ArcTerrainTileAccess::HasTile(*Terrain, EArcTerrainRail::Height, 0, TileKey);
			if (!bHasTile)
			{
				if (FlatCellIndex == INDEX_NONE)
				{
					FlatCellIndex = Cache->Cells.Num();
					Cache->Cells.Add(ArcTerrainSurface::BuildFlatCellCache(BaseHeightCm, FlatRenderQuads, Config.CellSize()));
				}
				Cache->KeyToIndex.Add(Key, FlatCellIndex);
				++CellsFlat;

				ArcPCGGetTerrainSettingsInternal::AccumulateCellLocalBounds(LocalBounds, X, Y, Config.CellSize(), BaseHeightCm, BaseHeightCm);
				bHaveLocalBounds = true;
				continue;
			}

			// LoadTileSync: PCG element execution is a one-shot bake, not the render residency queue, so
			// a blocking read here is the correct trade (the same exception the collision bootstrap takes).
			// An unreadable tile is a LEGAL state — the bake can fail non-fatally — so it is a skip with a
			// warning, never a check().
			TArray<float> HeightTexels;
			int32 TileRes = 0;
			if (!ArcTerrainTileAccess::LoadCellHeightsSync(*Terrain, Key, HeightTexels, TileRes) || TileRes < 4)
			{
				++CellsSkipped;
				Hasher.Update(&ArcPCGGetTerrainSettingsInternal::UnreadableCellHashTag, sizeof(ArcPCGGetTerrainSettingsInternal::UnreadableCellHashTag));
				Hasher.Update(&Key, sizeof(Key));
				PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("CellNoBakedHeight", "ArcTerrain cell ({0},{1}) is covered by a height tile that could not be read; leaving a hole."), X, Y));
				continue;
			}

			// TileRes is the COMPOSITE raster (CompositeQuads + 3), which may be coarser than the
			// dense raster the cell's mesh was emitted from — the tile's own quad count is what the
			// apron layout and the bilinear sampler must agree on, so derive it from the payload
			// rather than from Config.QuadsPerCell().
			const int32 RenderQuads = TileRes - 3;

			// The full apron-inclusive tile is what the apron layout below wants, and it already holds
			// terrain-local cm, so it goes to BuildCellCache as stored.
			float MinHeight = TNumericLimits<float>::Max();
			float MaxHeight = TNumericLimits<float>::Lowest();
			for (const float HeightCm : HeightTexels)
			{
				MinHeight = FMath::Min(MinHeight, HeightCm);
				MaxHeight = FMath::Max(MaxHeight, HeightCm);
			}

			const int32 Index = Cache->Cells.Num();
			Cache->Cells.Add(ArcTerrainSurface::BuildCellCache(Key, HeightTexels, RenderQuads, Config.CellSize()));
			Cache->KeyToIndex.Add(Key, Index);

			// Fold the exact payload this cell contributed: identity of the cell, its raster and its
			// bytes.
			Hasher.Update(&Key, sizeof(Key));
			Hasher.Update(&TileRes, sizeof(TileRes));
			Hasher.Update(HeightTexels.GetData(), HeightTexels.Num() * sizeof(float));

			// Accumulate terrain-local bounds: cell XY rect × observed height range (Z in cm).
			ArcPCGGetTerrainSettingsInternal::AccumulateCellLocalBounds(LocalBounds, X, Y, Config.CellSize(), MinHeight, MaxHeight);
			bHaveLocalBounds = true;

			++CellsRead;
		}
	}

	Cache->ContentHash = FIoHash(Hasher.Finalize());
	if (bHaveLocalBounds)
	{
		Cache->WorldBounds = LocalBounds.TransformBy(Cache->TerrainTransform);
	}

	UPCGTerrainData* Data = FPCGContext::NewObject_AnyThread<UPCGTerrainData>(Context);
	Data->Initialize(Cache, Settings->SamplingProperties);

	FPCGTaggedData& Tagged = Context->OutputData.TaggedData.Emplace_GetRef();
	Tagged.Data = Data;

	PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("ReadSummary", "ArcTerrain baked-tile read: {0} cells read, {1} flat at Base Height, {2} unreadable."), CellsRead, CellsFlat, CellsSkipped));
}

#undef LOCTEXT_NAMESPACE
