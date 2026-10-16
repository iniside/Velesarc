// Copyright Lukasz Baran. All Rights Reserved.

#include "Terrain/ArcTownPadStamp.h"

namespace ArcTownPadStampPrivate
{
	/** The pad's height comes from the component's Z, not from a texel, so the raster only has to exist. */
	constexpr int32 PadRasterDim = 2;
}

UArcTownPadStamp::UArcTownPadStamp()
{
	BlendMode = EArcTerrainBlendMode::Max;
	Priority = ArcTownPadStampPriority;
}

bool UArcTownPadStamp::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
	if (!PadContentId.IsValid())
	{
		return false;
	}

	OutDesc.Kind = EArcStampSourceKind::BakedField;
	OutDesc.ContentId = PadContentId;
	// SourceDims stays zero: its only consumers are the collision DDC key and the native-density
	// resolver, which reads it under the MeshPartitionAdaptive backend the pad does not select. The
	// terrain-wide weight-tile density cap reads FArcStampLayerDesc::SourceDims instead, and the pad
	// emits no weight layers at all.
	OutDesc.BakedProvider = [](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
	{
		OutDims = FIntPoint(ArcTownPadStampPrivate::PadRasterDim, ArcTownPadStampPrivate::PadRasterDim);
		OutValues.Init(0.0f, OutDims.X * OutDims.Y);
		return true;
	};
	OutDesc.SourceChannel = EArcTerrainSourceChannel::R;
	OutDesc.EncodingScale = 1.0f;
	OutDesc.ZeroInEncoding = 0.0f;
	OutDesc.EdgeFalloffBand = EdgeFalloffBand;
	return true;
}
