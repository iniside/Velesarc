// Copyright Lukasz Baran. All Rights Reserved.

#include "Weights/ArcTerrainWeightBuilder.h"

#if WITH_EDITOR

#include "Hash/Blake3.h"

namespace ArcTerrainWeightBuilderPrivate
{
	template <typename T>
	void HashPod(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}
}

FIoHash ArcTerrainWeights::ComputeCellWeightKey(const FIoHash& CompositeInputKey,
	TConstArrayView<FArcTerrainWeightLayerIdentity> PresentLayers,
	TOptional<uint32> BaseChannelNameHash, int32 WeightResolution)
{
	using namespace ArcTerrainWeightBuilderPrivate;

	FBlake3 Hash;

	// OWN FormatVersionKey, DELIBERATELY DISTINCT from ComputeCellCollisionKey's GUID (6C593D5C...):
	// the weight section is keyed independently, so a height-recipe change must not invalidate weights
	// and a weight-content/resolution change must not invalidate the height rail. THE PRODUCER IS PART
	// OF THE IDENTITY — bump this GUID on any weight-key-layout OR weight-composite-semantics change, or
	// sections composited by the old producer keep satisfying the key and sit next to new ones.
	// Bumped 2026-08-31: weight passes now carry the stamp's mask source, so the weight composite's
	// producer changed AND the per-layer mask identity below entered the key layout.
	static const FGuid FormatVersionKey(TEXT("4B0A6D71-2F3E-4C58-9A17-D5E8C4B39F60"));
	HashPod(Hash, FormatVersionKey);

	// The paired height composite-input identity (ArcTerrainCollision::ComputeCellCollisionKey for the
	// same GT stamp gather). A height/source-transform change that also moves weight footprints
	// invalidates weights through this. FIoHash is a raw 20-byte hash — no padding.
	HashPod(Hash, CompositeInputKey);

	// CompositeInputKey EXCLUDES weight-only layers, so the present weight-layer content is hashed
	// EXPLICITLY here. Count FIRST so a truncated vs full set can never collide. FIELD-WISE per layer
	// (never raw-struct — padding bytes would poison the key,
	// [[reference_ddc_keys_never_raw_struct_hash]]); the caller supplies a deterministic ChannelNameHash
	// order (stable, preserving composite z-order within a channel). THE NAME HASH, NEVER THE INDEX, and
	// the caller's order follows the same hash — otherwise a pure reorder permutes the hashed sequence
	// and moves the key anyway.
	const int32 NumLayers = PresentLayers.Num();
	HashPod(Hash, NumLayers);
	for (const FArcTerrainWeightLayerIdentity& Layer : PresentLayers)
	{
		HashPod(Hash, Layer.ChannelNameHash);
		HashPod(Hash, Layer.ContentId);     // FGuid — 16 bytes, no padding
		HashPod(Hash, Layer.SourceChannel); // uint8
		HashPod(Hash, Layer.EncodingScale);
		HashPod(Hash, Layer.ZeroInEncoding);
		// SourceDims hashed X/Y separately (never the raw FIntPoint/struct) mirroring the collision
		// key's HashSourceFields defense: a raster-shape change with an accidentally-retained
		// ContentId must still invalidate.
		HashPod(Hash, Layer.SourceDims.X);
		HashPod(Hash, Layer.SourceDims.Y);
		HashPod(Hash, static_cast<uint8>(Layer.Kind));
		// The stamp mask multiplies the composited weight, so a mask-only edit must invalidate. Same
		// field-wise discipline, X/Y separately.
		HashPod(Hash, static_cast<uint8>(Layer.MaskKind));
		HashPod(Hash, Layer.MaskContentId); // FGuid — 16 bytes, no padding
		HashPod(Hash, Layer.MaskSourceDims.X);
		HashPod(Hash, Layer.MaskSourceDims.Y);
	}

	// The BASE channel, which no layer above carries: the present set adds it by NAME and the composite
	// derives its plane as 1 - sum(others), so which name is base decides the plane SET and every other
	// plane's content. Presence flag FIRST — 0 is a legal name hash and can never stand for "no base".
	const uint8 bHasBaseChannel = BaseChannelNameHash.IsSet() ? 1 : 0;
	HashPod(Hash, bHasBaseChannel);
	if (BaseChannelNameHash.IsSet())
	{
		HashPod(Hash, BaseChannelNameHash.GetValue());
	}

	// The resolved weight raster resolution (apron-inclusive WeightTileResolution == WeightQuads+3,
	// bijective with the ResolveWeightQuads value): two composites of the same stamps at a different
	// WeightTexelSize must not share a key. Hashed like every other input (not the plane bytes).
	HashPod(Hash, WeightResolution);

	return Hash.Finalize();
}

#endif // WITH_EDITOR
