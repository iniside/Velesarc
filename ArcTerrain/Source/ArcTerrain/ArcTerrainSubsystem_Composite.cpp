// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainSubsystem.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainDebugDraw.h"
#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainSettings.h"
#include "ArcTerrainInvokers.h"
#include "ArcTerrainModule.h"
#include "Collision/ArcTerrainCollisionBuilder.h"
#include "Collision/ArcTerrainCollisionComponent.h"
#include "Collision/ArcTerrainHeightfieldRef.h"
#include "Composite/ArcTerrainCompositeShaders.h"
#include "Composite/ArcTerrainCpuComposite.h"
#include "DataMaps/ArcTerrainDataMapSample.h"
#include "Lighting/ArcTerrainFarOcclusionComponent.h"
#include "Lighting/ArcTerrainLightingComponent.h"
#include "Render/ArcTerrainCBTRenderer.h"
#include "Render/ArcTerrainCellMath.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "Render/ArcTerrainHeightBounds.h"
#include "Render/ArcTerrainRenderComponent.h"
#include "Render/ArcTerrainVertexFactory.h"
#include "Stamps/ArcTerrainMeshModifier.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Surface/ArcTerrainSurfaceSample.h"
#include "Weights/ArcTerrainWeightSlicePacking.h" // GArcTerrainWeightMaxSlices (atlas slice budget)
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h" // FTextureSource dims (native density resolve) + CPU source cache
#include "Engine/Texture2DArray.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/Optional.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PooledRenderTarget.h"
#include "RHIGPUReadback.h"
#include "RHIGlobals.h"
#include "RenderResource.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UObject/ObjectKey.h"

#if WITH_EDITOR
#include "ArcTerrainTileBake.h" // tile lattice + ComputeTileCacheKey
#include "ArcTerrainTileBlobs.h" // the three rails' DDC payloads
#include "ArcTerrainTileCache.h" // async DDC probe handle polling (height + weight bake rails)
#include "ArcTerrainTileIO.h"
#include "ArcTerrainTileNames.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "DataMaps/ArcTerrainDataMapBake.h"
#include "DataMaps/ArcTerrainDataMapKey.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "Subsystems/ImportSubsystem.h"
#include "Weights/ArcTerrainWeightBuilder.h" // FArcTerrainWeightPlane + ComputeCellWeightKey
#include "UObject/UnrealType.h" // FPropertyChangedEvent + GET_MEMBER_NAME_CHECKED (settings-changed handler)
#endif

#if WITH_EDITOR
// DEV-ONLY failure injection (the composite-failure guard exercise): forces every
// stamp GPU source resolve to fail on the GAME THREAD, so any stamped cell's composite drops its
// height passes and marks bCompositeFullyResolved = false.
//
// SCOPE — this covers the GT-RESOLVE failure class ONLY. The composite has a second, RENDER-side
// failure class this knob CANNOT reach: RenderCompositeCell silently skips a pass whose TextureRHI
// is invalid at render time (ArcTerrainCompositeShaders.cpp:133-135) AFTER the game thread already
// recorded bFullyResolved = true — and the source cache creates its RHI texture inside a render
// command (FArcTerrainSourceCache CreateAndUpload), so an upload failure there yields heights
// missing a stamp that the guard marks fully resolved. That pre-existing engine-side hole is
// UNGUARDED and unchanged by Step 4.
//
// OBSERVED vs INFERRED (2026-07-17 real-RHI probes, cells (0,0)/(1,0)/(0,1)/(-1,0)/(-1,-1)):
static int32 GArcTerrainDebugDropStampSources = 0;
static FAutoConsoleVariableRef CVarArcTerrainDebugDropStampSources(
	TEXT("arc.Terrain.DebugDropStampSources"), GArcTerrainDebugDropStampSources,
	TEXT("DEV: 1 = every stamp GPU source resolve FAILS, so composites of stamped cells set ")
	TEXT("bCompositeFullyResolved=false — exercises the never-persist guards (section builds must ")
	TEXT("report Skipped). ")
	TEXT("Default 0."),
	ECVF_Cheat);

static int32 GArcTerrainSkipWeightComposite = 0;
static FAutoConsoleVariableRef CVarArcTerrainSkipWeightComposite(
	TEXT("arc.Terrain.Weights.SkipComposite"), GArcTerrainSkipWeightComposite,
	TEXT("DEV/GPU-BISECT: 1 = skip the SYNCHRONOUS weight composite+readback (the only per-cell GPU work ")
	TEXT("gated on a cell HAVING painted weight channels, and the only path that submits and blocks mid-")
	TEXT("enqueue). Geometry is unaffected; existing weight sections are left intact. Default 0."),
	ECVF_Cheat);

static int32 GArcTerrainDropBakedFieldSources = 0;
static FAutoConsoleVariableRef CVarArcTerrainDropBakedFieldSources(
	TEXT("arc.Terrain.DropBakedFieldSources"), GArcTerrainDropBakedFieldSources,
	TEXT("DEV/GPU-BISECT: 1 = BakedField stamp sources (river, lake, town pads) never resolve, so the ")
	TEXT("R32F provider raster is neither uploaded nor sampled. Texture stamps are untouched. Cells ")
	TEXT("covered by a baked-field stamp then composite unresolved and never persist. Default 0."),
	ECVF_Cheat);
#endif

namespace ArcTerrainSubsystemPrivate
{
	// Resolve a stamp raster source to its cached GPU texture (null on failure/no-op kinds).
	FArcTerrainSourceTexturePtr ResolveSource(FArcTerrainSourceCache& Cache, EArcStampSourceKind Kind,
		const FGuid& ContentId, UTexture2D* Texture, const TFunction<bool(TArray<float>&, FIntPoint&)>& Provider,
		EArcTerrainSourceChannel SourceChannel)
	{
#if WITH_EDITOR
		if (GArcTerrainDebugDropStampSources != 0 && Kind != EArcStampSourceKind::None)
		{
			// Failure injection (see the cvar): a null return here is byte-identical to a real
			// GAME-THREAD resolve failure (missing texture / provider refusal) — the callers mark
			// bCompositeFullyResolved = false. It does NOT model a render-side upload failure; that
			// class happens after this function returned a valid entry (see the cvar comment).
			return nullptr;
		}
		if (GArcTerrainDropBakedFieldSources != 0 && Kind == EArcStampSourceKind::BakedField)
		{
			return nullptr;
		}
#endif
		switch (Kind)
		{
		case EArcStampSourceKind::Texture:
#if WITH_EDITORONLY_DATA
			return (Texture != nullptr) ? Cache.GetOrCreateFromTexture(*Texture, SourceChannel, ContentId) : nullptr;
#else
			return nullptr; // no FTextureSource outside the editor — see FArcTerrainSourceCache
#endif
		case EArcStampSourceKind::BakedField:
			return Cache.GetOrCreateFromProvider(ContentId, Provider);
		default:
			return nullptr;
		}
	}

	void FinalizeCompositeAttempt(FArcTerrainState& State, FArcTerrainCellKey Key, FArcTerrainCellHot& Hot,
		FArcTerrainCellState& Cell, double Now)
	{
		Hot.bDirty = 0;
		Cell.bCompositeResolveRetryPending = !Cell.bCompositeFullyResolved;
		if (Cell.bCompositeFullyResolved)
		{
			Cell.CompositeResolveRetryAfter = 0.0;
			Cell.CompositeResolveRetryCount = 0;
			State.CompositeRetryCells.Remove(Key);
			return;
		}

		Cell.CompositeResolveRetryAfter = ScheduleExponentialRetry(Cell.CompositeResolveRetryCount, 0.1, Now);
		State.CompositeRetryCells.Add(Key);
		if (State.NextCompositeRetry <= 0.0 || Cell.CompositeResolveRetryAfter < State.NextCompositeRetry)
		{
			State.NextCompositeRetry = Cell.CompositeResolveRetryAfter;
		}
	}
	// Fill the geometry + falloff fields shared by every pass of one stamp.
	void FillCommonPassFields(ArcTerrainComposite::FStampPass& Pass, const FArcStampSourceDesc& Desc,
		const FMatrix44f& LocalToPatch, const FVector2f& CoverageXY)
	{
		Pass.LocalToPatch = LocalToPatch;
		// DF stamp rebase origin (infinite-plane Step 2): the terrain-local (z=0) point mapping to
		// patch-local origin, P0 = -T * L^-1 (row-vector convention) over the SAME float matrix the
		// shader applies — L = the upper-left 2x2 linear block (full scale+shear+rotation, rows
		// M[0].xy / M[1].xy; the z row multiplies z=0 and drops out), T = the xy translation row
		// M[3].xy. Solved in double from the exact float entries; deterministic and cell-independent
		// (LocalToPatch comes from StampToTerrain only), so adjacent cells derive identical bits.
		{
			const double A00 = static_cast<double>(LocalToPatch.M[0][0]);
			const double A01 = static_cast<double>(LocalToPatch.M[0][1]);
			const double A10 = static_cast<double>(LocalToPatch.M[1][0]);
			const double A11 = static_cast<double>(LocalToPatch.M[1][1]);
			const double Tx = static_cast<double>(LocalToPatch.M[3][0]);
			const double Ty = static_cast<double>(LocalToPatch.M[3][1]);
			const double Det = A00 * A11 - A01 * A10;
			// Degenerate (zero-scale) stamp: keep the origin finite — a NaN/inf param would defeat
			// the shader's footprint reject (NaN compares false), while 0 reproduces the old
			// singular-matrix behaviour (finite garbage that still fails the footprint test).
			if (FMath::Abs(Det) > 1.0e-30)
			{
				Pass.PatchOriginTerrainLocal = FVector2d(
					(-A11 * Tx + A10 * Ty) / Det,
					(A01 * Tx - A00 * Ty) / Det);
			}
			else
			{
				Pass.PatchOriginTerrainLocal = FVector2d::ZeroVector;
			}
		}
		Pass.CoverageXY = CoverageXY;
		Pass.EdgeFalloffBand = Desc.EdgeFalloffBand;
		Pass.EdgeSharpness = Desc.EdgeSharpness;
		Pass.CornerRadius = Desc.CornerRadius;
		Pass.bCircleMask = Desc.bCircleMask ? 1 : 0;
		Pass.CircleCenterOffset = FVector2f(Desc.CircleCenterOffset);
		Pass.CircleRadius = Desc.CircleRadius;
		Pass.CircleFalloffBand = Desc.CircleFalloffBand;
	}

	// THE SHARED COMPOSITE-JOB BUILDER (runtime-detail Step 3): the stamp-gather + pass-build both
	// composite variants run — the persistent cell composite (CompositeCell, which owns every piece
	// of cell-state mutation around this call) and the transient bake composite
	// (UArcTerrainSubsystem::CompositeTransientBakeTile, which must mutate NO cell state and returns
	// the resolve status BY VALUE through this function's return).
	//
	// The CALLER owns the destination bindings: OutJob's atlas slice / HeightTileResource / Scratch* /
	// WeightRTResources must be set BEFORE the call; this fills TileResolution/TexelOrigin/CellSize from Quads and appends the
	// ordered stamp passes. A weight pass is emitted only when its channel's RGBA8 slot
	// (ChannelIndex / 4) is BOUND — null holes in WeightRTResources are legal and skip the slot's
	// channels (the transient bake variant binds none; RenderCompositeCell tolerates the holes).
	// CARVE passes are height passes and are always emitted.
	//
	// Returns bFullyResolved: false when a height or carve source failed to resolve, or when the height
	// pass had to drop the stamp mask it should have carried. A weight-pass mask drop does not affect it.
	bool BuildCompositeJob(FArcTerrainSourceCache& SourceCache, const FArcTerrainState& State,
		const FArcTerrainConfig& Config, const FTransform& TerrainTransform, FArcTerrainCellKey Key,
		int32 Quads, int32 WeightQuads, float BaseHeightCm,
		ArcTerrainComposite::FCellCompositeJob& OutJob)
	{
		OutJob.TileResolution = Quads + 3;
		OutJob.BaseHeightCm = BaseHeightCm;
		OutJob.CellKey = Key;
		const int32 BaseChannelIndex = ArcTerrain::ResolveBaseChannelIndex(
			Config.ChannelSet != nullptr ? Config.ChannelSet->Channels.Num() : 0);
		OutJob.BaseWeightArrayIndex =
			(BaseChannelIndex != INDEX_NONE) ? (BaseChannelIndex / 4) : INDEX_NONE;
		OutJob.bLogWeightVerbose = ArcTerrainDiag::IsWeightCompositeVerbose();
		// Texel spacing follows from TileResolution in the composite shader
		// (CellSize / (TileResolution - 3) = CellSize / Quads). Shared-edge AND apron texels get the
		// SAME global index in both neighbouring cells at matching density, making their composited
		// values bit-identical.
		OutJob.TexelOrigin = ComputeGlobalTexelOrigin(Key, Quads);
		OutJob.CellSize = static_cast<float>(Config.CellSize());

		// Weight tile (weightmap-decouple Stage 1 Step 1): its own resolution + global texel origin at
		// WeightQuads, decoupled from the height grid, so the shared border weight-texel takes the SAME
		// global index in both neighbouring cells -> bit-identical composited value (plan I1);
		// WeightQuads is uniform per terrain (ResolveWeightQuads). The bake path passes
		// WeightQuads == Quads, keeping its metric weight tiles at height res (S1).
		OutJob.WeightTileResolution = WeightQuads + 3;
		OutJob.WeightTexelOrigin = ComputeGlobalTexelOrigin(Key, WeightQuads);

		// SPANS THE RASTERS, not the cell rectangle: the apron texels carry the neighbour's global indices,
		// so a stamp reaching only into the apron contributes to a texel this cell writes. Gathering over
		// the exact rectangle would give that shared global texel a different contributor set in each of
		// the two cells — a seam. The union covers both rasters, whose apron pads differ with their
		// densities.
		FBox CellBounds = ArcTerrain::CellLocalBounds(Key, Config);
		if (Quads > 0)
		{
			CellBounds += RasterGatherBounds(OutJob.TexelOrigin, OutJob.TileResolution,
				Config.CellSize() / static_cast<double>(Quads));
		}
		if (WeightQuads > 0)
		{
			CellBounds += RasterGatherBounds(OutJob.WeightTexelOrigin, OutJob.WeightTileResolution,
				Config.CellSize() / static_cast<double>(WeightQuads));
		}

		// Gather intersecting stamps: edge-inclusive (FBox::Intersect is closed-interval), skipping
		// disabled and stale ones. Sorted ascending by the shared order authority.
		TArray<UArcTerrainStampComponent*> SortedStamps;
		GatherSortedStamps(State, CellBounds, SortedStamps);

		bool bFullyResolved = true;
		for (UArcTerrainStampComponent* Stamp : SortedStamps)
		{
			FArcStampSourceDesc Desc;
			if (!Stamp->GetSourceDescriptor(Desc))
			{
				continue;
			}

			// Stamp placement relative to the terrain frame. Height values are stamp-local cm: the
			// stamp's Z scale multiplies them; for ABSOLUTE blend targets (AlphaBlend/Min/Max) the
			// stamp's terrain-local Z offset shifts them (Additive stays a pure delta).
			const FTransform StampToTerrain =
				Stamp->GetComponentTransform().GetRelativeTransform(TerrainTransform);
			// Exact inverse: FTransform::Inverse() is lossy for rotation ∘ non-uniform scale;
			// ToInverseMatrixWithScale matches the MP reference's InverseTransformPosition math.
			const FMatrix44f LocalToPatch = FMatrix44f(StampToTerrain.ToInverseMatrixWithScale());
			const FVector2f CoverageXY = FVector2f(Stamp->CoverageXY);
			const float RelScaleZ = static_cast<float>(StampToTerrain.GetScale3D().Z);
			const float StampLocalZ = static_cast<float>(StampToTerrain.GetTranslation().Z);
			const EArcTerrainBlendMode BlendMode = Stamp->BlendMode;

			// Resolved once per stamp: height and weight passes both may need it (carve does not).
			FArcTerrainSourceTexturePtr StampMaskSource;
			if (Desc.MaskKind != EArcStampSourceKind::None)
			{
				StampMaskSource = ResolveSource(SourceCache, Desc.MaskKind, Desc.MaskContentId,
					Desc.MaskTexture, Desc.MaskBakedProvider, Desc.MaskSourceChannel);
			}

			// Height pass.
			if (Desc.Kind != EArcStampSourceKind::None)
			{
				FArcTerrainSourceTexturePtr Source = ResolveSource(
					SourceCache, Desc.Kind, Desc.ContentId, Desc.Texture, Desc.BakedProvider, Desc.SourceChannel);
				if (!Source.IsValid())
				{
					bFullyResolved = false; // height pass dropped — result must not be cached/persisted
				}
				else
				{
					ArcTerrainComposite::FStampPass& Pass = OutJob.Passes.AddDefaulted_GetRef();
					FillCommonPassFields(Pass, Desc, LocalToPatch, CoverageXY);
					Pass.Target = ArcTerrainComposite::FStampPass::ETarget::Height;
					Pass.BlendMode = BlendMode;
					// Softness is authored in stamp-local Z cm; tile heights are terrain-local — scale
					// by the stamp's relative Z scale (MP applies softness in patch-local units).
					Pass.MinMaxSoftness = Stamp->MinMaxSoftness * RelScaleZ;
					Pass.Source = Source;
					Pass.EncodingScale = Desc.EncodingScale;
					// Height only: the weight and carve passes below keep their authored zero point.
					Pass.ZeroInEncoding = Desc.bDeriveZeroFromBorder ? Source->BorderMean : Desc.ZeroInEncoding;
					Pass.ValueScale = RelScaleZ;
					Pass.ValueBias = (BlendMode == EArcTerrainBlendMode::Additive) ? 0.0f : StampLocalZ;
					ComputeSampleMap(Source->Dims, Pass.UVScale, Pass.UVBias);

					if (Desc.MaskKind != EArcStampSourceKind::None)
					{
						if (StampMaskSource.IsValid() && StampMaskSource->Dims == Source->Dims)
						{
							Pass.MaskSource = StampMaskSource;
						}
						else
						{
							const FIntPoint MaskDims = StampMaskSource.IsValid() ? StampMaskSource->Dims : FIntPoint::ZeroValue;
							UE_LOGF(LogArcTerrain, Warning,
								"[HeightComposite] (%d,%d) stamp '%ls' mask DROPPED: mask dims %dx%d do not "
								"match value dims %dx%d (0x0 = mask source failed to resolve). "
								"Result must not be cached/persisted.",
								Key.X, Key.Y, *Stamp->GetName(), MaskDims.X, MaskDims.Y,
								Source->Dims.X, Source->Dims.Y);
							bFullyResolved = false;
						}
					}
				}
			}

			// Weight layers (+ optional carve into height).
			for (const TPair<FName, FArcStampLayerDesc>& LayerPair : Desc.Layers)
			{
				const FArcStampLayerDesc& Layer = LayerPair.Value;

				// A layer whose channel doesn't resolve against the terrain's channel set is dropped
				// ENTIRELY — no weight pass AND no carve. Checked BEFORE ResolveSource so a dropped
				// layer never uploads its raster to the GPU source cache.
				const int32 ChannelIndex = Config.ChannelSet ? Config.ChannelSet->FindChannelIndex(LayerPair.Key) : INDEX_NONE;
				if (ChannelIndex == INDEX_NONE)
				{
					if (ArcTerrainDiag::IsWeightCompositeVerbose())
					{
						UE_LOGF(LogArcTerrain, Warning,"[WeightComposite] (%d,%d) stamp '%ls' layer channel '%ls' DROPPED: %ls. No weight pass, no carve.",
							Key.X, Key.Y, *Stamp->GetName(), *LayerPair.Key.ToString(),
							Config.ChannelSet ? TEXT("channel not found in terrain ChannelSet")
											  : TEXT("terrain has no ChannelSet assigned"));
					}
					continue;
				}

				// Slot-bound (see the function comment): the persistent variant binds every slot; the
				// transient bake variant binds none, so it emits height + carve passes only.
				const bool bEmitWeightPass = OutJob.WeightRTResources.IsValidIndex(ChannelIndex / 4)
					&& OutJob.WeightRTResources[ChannelIndex / 4] != nullptr;

				const bool bWantCarve = Layer.bContributeToHeight && Layer.HeightAmplitude != 0.0f;
				if (!bEmitWeightPass && !bWantCarve)
				{
					if (ArcTerrainDiag::IsWeightCompositeVerbose())
					{
						UE_LOGF(LogArcTerrain, Warning,
							"[WeightComposite] (%d,%d) stamp '%ls' channel '%ls' (index %d) DROPPED: "
							"weight-RT slot %d not bound and no carve -- layer feeds nothing here.",
							Key.X, Key.Y, *Stamp->GetName(), *LayerPair.Key.ToString(), ChannelIndex, ChannelIndex / 4);
					}
					continue; // layer feeds nothing here — skip the upload too
				}

				FArcTerrainSourceTexturePtr Source = ResolveSource(
					SourceCache, Layer.Kind, Layer.ContentId, Layer.Texture, Layer.BakedProvider, Layer.SourceChannel);
				// A dropped CARVE source poisons the height-relevant resolve; a dropped weight-only
				// source does not.
				bFullyResolved = bFullyResolved && (Source.IsValid() || !bWantCarve);
				if (!Source.IsValid())
				{
					if (ArcTerrainDiag::IsWeightCompositeVerbose())
					{
						UE_LOGF(LogArcTerrain, Warning,
							"[WeightComposite] (%d,%d) stamp '%ls' channel '%ls' (index %d) DROPPED: "
							"GPU source failed to resolve (missing/invalid weight texture).",
							Key.X, Key.Y, *Stamp->GetName(), *LayerPair.Key.ToString(), ChannelIndex);
					}
					continue;
				}

				if (ArcTerrainDiag::IsWeightCompositeVerbose())
				{
					UE_LOGF(LogArcTerrain, Log,"[WeightComposite] (%d,%d) stamp '%ls' channel '%ls' (index %d) EMIT: weightPass=%ls carve=%ls.",
						Key.X, Key.Y, *Stamp->GetName(), *LayerPair.Key.ToString(), ChannelIndex,
						bEmitWeightPass ? TEXT("yes") : TEXT("no"), bWantCarve ? TEXT("yes") : TEXT("no"));
				}

				if (bEmitWeightPass)
				{
					ArcTerrainComposite::FStampPass& Pass = OutJob.Passes.AddDefaulted_GetRef();
					FillCommonPassFields(Pass, Desc, LocalToPatch, CoverageXY);
					Pass.Target = ArcTerrainComposite::FStampPass::ETarget::Weight;
					Pass.WeightRTIndex = ChannelIndex / 4;
					Pass.WeightChannel = ChannelIndex % 4;
					Pass.Source = Source;
					Pass.EncodingScale = Layer.EncodingScale;
					Pass.ZeroInEncoding = Layer.ZeroInEncoding;
					Pass.ValueScale = 1.0f;
					Pass.ValueBias = 0.0f;
					ComputeSampleMap(Source->Dims, Pass.UVScale, Pass.UVBias);

					if (Desc.MaskKind != EArcStampSourceKind::None)
					{
						if (StampMaskSource.IsValid() && StampMaskSource->Dims == Source->Dims)
						{
							Pass.MaskSource = StampMaskSource;
						}
						else
						{
							const FIntPoint MaskDims = StampMaskSource.IsValid() ? StampMaskSource->Dims : FIntPoint::ZeroValue;
							UE_LOGF(LogArcTerrain, Warning,
								"[WeightComposite] (%d,%d) stamp '%ls' channel '%ls' (index %d) mask DROPPED: "
								"mask dims %dx%d do not match value dims %dx%d (0x0 = mask source failed to "
								"resolve).",
								Key.X, Key.Y, *Stamp->GetName(), *LayerPair.Key.ToString(), ChannelIndex,
								MaskDims.X, MaskDims.Y, Source->Dims.X, Source->Dims.Y);
						}
					}

					// Decode + UV + localization trace: everything that turns the source texel into a
					// composited weight. SourceDims=0 -> source didn't resolve; UVScale/Bias map the cell's
					// stamp-local coords into the source texture (bad UV -> samples outside -> 0/garbage);
					// EncodingScale/ZeroInEncoding decode the texel; CoverageXY is the stamp footprint half-size.
					if (ArcTerrainDiag::IsWeightCompositeVerbose())
					{
						UE_LOGF(LogArcTerrain, Warning,
							"[WeightPass] (%d,%d) ch '%ls' idx %d: SourceDims=%dx%d SourceChannel=%d "
							"Encode(Scale=%.3f Zero=%.3f) UVScale=(%.4f,%.4f) UVBias=(%.4f,%.4f) "
							"CoverageXY=(%.1f,%.1f)",
							Key.X, Key.Y, *LayerPair.Key.ToString(), ChannelIndex,
							Source->Dims.X, Source->Dims.Y, static_cast<int32>(Layer.SourceChannel),
							Layer.EncodingScale, Layer.ZeroInEncoding,
							static_cast<double>(Pass.UVScale.X), static_cast<double>(Pass.UVScale.Y),
							static_cast<double>(Pass.UVBias.X), static_cast<double>(Pass.UVBias.Y),
							static_cast<double>(CoverageXY.X), static_cast<double>(CoverageXY.Y));
					}
				}

				// Carve: fold the weight raster into height as a signed additive delta — always
				// Additive regardless of the stamp's blend mode, applied AFTER the main height pass
				// (legacy BlendHeight contract: carve is a separate delta on top).
				if (bWantCarve)
				{
					ArcTerrainComposite::FStampPass& Pass = OutJob.Passes.AddDefaulted_GetRef();
					FillCommonPassFields(Pass, Desc, LocalToPatch, CoverageXY);
					Pass.Target = ArcTerrainComposite::FStampPass::ETarget::Height;
					Pass.BlendMode = EArcTerrainBlendMode::Additive;
					Pass.Source = Source;
					Pass.EncodingScale = Layer.EncodingScale;
					Pass.ZeroInEncoding = Layer.ZeroInEncoding;
					Pass.ValueScale = Layer.HeightAmplitude * RelScaleZ;
					Pass.ValueBias = 0.0f;
					ComputeSampleMap(Source->Dims, Pass.UVScale, Pass.UVBias);
				}
			}
		}
		return bFullyResolved;
	}
}

bool UArcTerrainSubsystem::CompositeCell(AArcTerrain& Terrain, FArcTerrainState& State, FArcTerrainCellKey Key,
	FArcTerrainCellState& Cell, UTextureRenderTarget2D** OutRetainedHeightTile)
{
	using namespace ArcTerrainSubsystemPrivate;

	const FArcTerrainConfig& Config = Terrain.Config;
	if (Config.QuadsPerCell() < 1 || Config.CellSize() <= 0.0)
	{
		return false;
	}
	// STEP 5: the raster is CompositeQuads — uniform across every cell and every camera position, no
	// longer the cell's render density. Tile carries a 1-texel apron on every side: (CompositeQuads + 1)
	// inner texels + 2 apron texels. The apron reuses the neighbour cell's global texel indices
	// (bit-identical values) so edge-gradient consumers (VF normals, PS taps) have a valid +-1 texel
	// neighbourhood across the cell border. With one uniform density, shared-edge and apron texels take
	// the same global index in both neighbours unconditionally — the bit-identical-seam guarantee no
	// longer rests on the old "base texel i coincides with hi texel M*i" power-of-two argument, because
	// there is no second density left to coincide with.
	const int32 RenderQuads = ArcTerrainSubsystemPrivate::CompositeQuads(Config);
	const int32 TileResolution = RenderQuads + 3;
	const int32 NumChannels = Config.ChannelSet ? Config.ChannelSet->Channels.Num() : 0;
	const int32 NumWeightRTs = FMath::DivideAndRoundUp(NumChannels, 4);

	// Weight-composite density is DECOUPLED from the mesh raster (weightmap-decouple Stage 1 Step 1):
	// a high-res source weightmap composites at its own texel size, independent of RenderQuads.
	// Uniform per terrain (ResolveWeightQuads) so the cross-cell weight seam stays bit-identical (I1).
	const int32 WeightQuads = ArcTerrainSubsystemPrivate::ResolveWeightQuads(Config, State);
	const int32 WeightTileResolution = WeightQuads + 3;

	// THE HEIGHT DESTINATION. The cell's sector must already hold a slice; a sector still waiting for
	// admission has nowhere to publish, so the composite is DEFERRED (the cell stays dirty) rather than
	// run into a buffer nothing reads. Admission consults no composite state (see IsCellPublishable), so
	// a deferred composite cannot gate the slice it is waiting for.
	TSharedPtr<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe> AtlasResources;
	int32 AtlasHeightSlice = INDEX_NONE;
	FIntPoint AtlasDestOrigin = FIntPoint::ZeroValue;
	if (State.TileAtlas.IsValid() && State.TileAtlas->GetResources().IsValid())
	{
		const FArcTerrainTileAtlasLayout& Layout = State.TileAtlas->GetLayout();
		if (Layout.IsValid() && Layout.HeightQuads + 3 == TileResolution)
		{
			const FArcTerrainSectorId SectorId{
				0, ArcTerrainAtlas::GroupKeyFromCell(Key, Layout.SectorShift) };
			const int32 Slice = State.TileAtlas->FindHeightSlice(SectorId);
			if (Slice != INDEX_NONE)
			{
				AtlasResources = State.TileAtlas->GetResources();
				AtlasHeightSlice = Slice;
				AtlasDestOrigin = ArcTerrainAtlas::HeightTileOriginInSlice(SectorId, Key, Layout);
			}
		}
	}

	if (AtlasHeightSlice == INDEX_NONE && OutRetainedHeightTile == nullptr)
	{
		return false;
	}

	Cell.TileSet.Acquire(RenderTargetPool, NumWeightRTs, WeightTileResolution);

	UTextureRenderTarget2D* HeightTile = nullptr;
	if (OutRetainedHeightTile != nullptr)
	{
		HeightTile = RenderTargetPool.Acquire(TileResolution, RTF_R32f);
		if (HeightTile == nullptr)
		{
			return false;
		}
	}

	// Two pooled R32f scratch tiles for the height ping-pong chain. Releasing them right after the
	// enqueue is safe: only render commands touch RT contents and they execute serially, so any
	// later composite that re-acquires these RTs runs strictly after this one on the render thread.
	UTextureRenderTarget2D* ScratchA = RenderTargetPool.Acquire(TileResolution, RTF_R32f);
	UTextureRenderTarget2D* ScratchB = RenderTargetPool.Acquire(TileResolution, RTF_R32f);

	ArcTerrainComposite::FCellCompositeJob Job;
	Job.AtlasResources = AtlasResources;
	Job.AtlasHeightSlice = AtlasHeightSlice;
	Job.AtlasHeightDestOrigin = AtlasDestOrigin;
	Job.HeightTileResource = HeightTile != nullptr
		? HeightTile->GameThread_GetRenderTargetResource() : nullptr;
	Job.ScratchAResource = ScratchA->GameThread_GetRenderTargetResource();
	Job.ScratchBResource = ScratchB->GameThread_GetRenderTargetResource();
	Job.WeightRTResources.Reserve(Cell.TileSet.WeightRTs.Num());
	for (UTextureRenderTarget2D* WeightRT : Cell.TileSet.WeightRTs)
	{
		Job.WeightRTResources.Add(WeightRT->GameThread_GetRenderTargetResource());
	}

	// Stamp gather + ordered pass build — shared with the transient bake composite (Step 3
	// extracted it; see BuildCompositeJob). bFullyResolved: false when any HEIGHT-relevant pass
	// (height source or carve) dropped because its GPU source failed to resolve
	// (see FArcTerrainCellState::bCompositeFullyResolved).
	const bool bFullyResolved = BuildCompositeJob(SourceCache, State, Config,
		Terrain.GetTerrainTransform(), Key, RenderQuads, WeightQuads, Terrain.HeightOptions.BaseHeightCm,
		Job);

	// Version bump on the game thread AT enqueue: the publication tag below is stamped from it.
	++Cell.Version;
	Cell.bCompositeFullyResolved = bFullyResolved;
	// THE PUBLICATION TAG, written here because this job is what writes the slice. The render command
	// below is enqueued before any readback the consumers issue, so a game-thread claim made now is
	// ordered correctly against them on the render thread.
	Cell.AtlasHeightVersion = AtlasHeightSlice != INDEX_NONE ? Cell.Version : 0;
	Cell.bAtlasRefillPending = true;
	State.AtlasSchedule.DirtyAtlasCells.Add(Key);
	if (State.TileAtlas.IsValid())
	{
		const FArcTerrainTileAtlasLayout& AtlasLayout = State.TileAtlas->GetLayout();
		const FArcTerrainWeightGroupId WeightGroupId{
			0, ArcTerrainAtlas::GroupKeyFromCell(Key, AtlasLayout.WeightSliceShift) };
		State.AtlasSchedule.DirtyWeightGroups.Add(WeightGroupId);
		const FArcTerrainSectorId HeightSectorId{
			0, ArcTerrainAtlas::GroupKeyFromCell(Key, AtlasLayout.SectorShift) };
		if (State.AtlasSchedule.PendingHeightAdmissions.Contains(HeightSectorId))
		{
			State.AtlasSchedule.WokenHeightAdmissions.Add(HeightSectorId);
		}
	}

	ENQUEUE_RENDER_COMMAND(ArcTerrainCompositeCell)(
		[Job = MoveTemp(Job)](FRHICommandListImmediate& RHICmdList)
		{
			ArcTerrainComposite::RenderCompositeCell(RHICmdList, Job);
		});

	RenderTargetPool.Release(ScratchA);
	RenderTargetPool.Release(ScratchB);
	if (OutRetainedHeightTile != nullptr)
	{
		*OutRetainedHeightTile = HeightTile;
	}
	return true;
}
