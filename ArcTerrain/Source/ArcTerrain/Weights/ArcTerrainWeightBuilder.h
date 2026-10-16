// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoHash.h" // ComputeCellWeightKey
#include "Misc/Optional.h" // ComputeCellWeightKey's BaseChannelNameHash
#include "Weights/ArcTerrainWeightSlicePacking.h" // GArcTerrainWeightMaxSlices (atlas slice budget)
#include "Stamps/ArcTerrainStampComponent.h" // EArcStampSourceKind (FArcTerrainWeightLayerIdentity::Kind)

#if WITH_EDITOR

/**
 * Weightmap-decouple Stage 2 Step 3 (docs/plans/2026-07-20-2051-arcterrain-weightmap-decouple-plan.md):
 * the TEXTURING-ONLY weight raster + per-cell channel/slice table produced at BAKE time. Zero
 * tessellation coupling — the deleted tessellation-metric weight machinery
 * (OutWeightRasters / ReadbackWeightsSync / native composite / PretendLayers) is NOT resurrected.
 * These types are the physical home for the persisted weight path, deliberately separate from the
 * height rail.
 *
 * Editor-only: the whole weight bake is a build-time step, exactly like the height section build. The
 * Step-4 consumer turns these into a UTexture2DArray + weight section.
 */
namespace ArcTerrainWeights
{
	/**
	 * One texturing-only weight raster read back from the RGBA8 composite at the independent WeightQuads
	 * resolution (weightmap-decouple Stage 1). Row-major (WeightQuads+3)^2 uint8 — apron included so the
	 * cross-cell weight seam (I1) is preserved into the persisted array — lossless vs the 8-bit composite
	 * RTs, ready to blit straight into a Step-4 TSF_G8 UTexture2DArray slice.
	 */
	struct FArcTerrainWeightPlane
	{
		/** Global channel index in UArcTerrainChannelSet order (RT slot = ChannelIndex/4, RGBA sub-channel = ChannelIndex%4). */
		int32 ChannelIndex = INDEX_NONE;

		/** Row-major, TileResolution*TileResolution entries (TileResolution == WeightQuads+3). */
		TArray<uint8> G8;
	};

	/**
	 * Source identity of ONE present weight layer covering a cell — the texturing-only inputs
	 * ComputeCellWeightKey (Stage 2 Step 5) hashes so a weight-content or encoding change re-bakes the
	 * weight section INDEPENDENTLY of the mesh key. Deliberately mirrors the height-relevant fields
	 * ComputeCellCollisionKey folds, but for the weight-only layers that key EXCLUDES
	 * (BuildCompositeJob excludes them). One entry per (covering stamp, Desc.Layers pair) that resolves
	 * to a valid terrain channel; the caller emits them in a deterministic order (stable by
	 * ChannelNameHash).
	 */
	struct FArcTerrainWeightLayerIdentity
	{
		/** ArcTerrain::ChannelNameHash of the layer's channel name — the identity the key hashes, so
		 *  reordering the channel set moves neither the hashed value nor the order it is hashed in. */
		uint32 ChannelNameHash = 0;

		/** Raster content identity (FArcStampLayerDesc::ContentId) — a source-raster change moves this. */
		FGuid ContentId;

		/** static_cast<uint8>(FArcStampLayerDesc::SourceChannel) — the decoded texel channel. */
		uint8 SourceChannel = 0;

		/** FArcStampLayerDesc decode: weight = (texel - ZeroInEncoding) * EncodingScale. */
		float EncodingScale = 1.0f;
		float ZeroInEncoding = 0.0f;

		/** FArcStampLayerDesc::SourceDims — raster shape. Folded into the key so a shape change with an
		 *  accidentally-retained ContentId still invalidates (mirrors the collision key's SourceDims
		 *  defense, ArcTerrainCollisionBuilder.cpp HashSourceFields). */
		FIntPoint SourceDims = FIntPoint::ZeroValue;

		/** FArcStampLayerDesc::Kind (Texture vs BakedField) — the two source kinds decode differently. */
		EArcStampSourceKind Kind = EArcStampSourceKind::None;

		/** The owning stamp's FArcStampSourceDesc mask (shared by every layer of that stamp), which the
		 *  weight composite multiplies into the falloff. */
		EArcStampSourceKind MaskKind = EArcStampSourceKind::None;
		FGuid MaskContentId;
		FIntPoint MaskSourceDims = FIntPoint::ZeroValue;
	};

	/**
	 * Build key of one cell's PERSISTED WEIGHT SECTION (weightmap-decouple Stage 2 Step 5). Its OWN
	 * content key so an unchanged cell is not re-persisted, and a WeightTexelSize or weight-layer-content
	 * change re-bakes weights INDEPENDENTLY of any other rail. Isolated exactly like the sibling
	 * ComputeCellCollisionKey — its own fresh FormatVersionKey GUID.
	 *
	 * Hashes the INPUTS (never the composited plane bytes — GPU-wobble-sensitive), FIELD-WISE (never
	 * raw-struct — padding garbage → [[reference_ddc_keys_never_raw_struct_hash]]): the format GUID, then
	 * the height CompositeInputKey (a height/source-transform change that also moves weight footprints
	 * invalidates), then — because CompositeInputKey EXCLUDES weight-only layers — each present weight
	 * layer's source identity in the caller's deterministic ChannelNameHash order, then the BASE channel's
	 * name hash, then the resolved weight raster resolution (WeightQuads+3).
	 *
	 * BaseChannelNameHash is ArcTerrain::ChannelNameHash of the channel set's FIRST entry, unset when the
	 * terrain has no channel set or an empty one. It is an input in its own right because no layer carries
	 * it: the base channel is added to the present set by name and the composite derives its plane as
	 * 1 - sum(others), so a pure reorder that changes WHICH NAME is base changes the tile's plane SET and
	 * every other plane's content while every layer identity above stays put.
	 *
	 * A weight-only stamp's PLACEMENT rides in CompositeInputKey: ComputeCellCollisionKey hashes class
	 * path, relative transform, priority, blend, softness and coverage for EVERY stamp of the cell's
	 * SortedStamps (Collision/ArcTerrainCollisionBuilder.cpp) before it reaches the descriptor; only a
	 * weight-only stamp's LAYER identity is excluded, and that is what the layer list above covers.
	 */
	ARCTERRAIN_API FIoHash ComputeCellWeightKey(const FIoHash& CompositeInputKey,
		TConstArrayView<FArcTerrainWeightLayerIdentity> PresentLayers,
		TOptional<uint32> BaseChannelNameHash, int32 WeightResolution);
}

#endif // WITH_EDITOR
