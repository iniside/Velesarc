// Copyright Lukasz Baran. All Rights Reserved.

#include "Assets/ArcTerrainPatchAsset.h"

#if WITH_EDITOR
#include "Engine/Texture2D.h"
#include "Stamps/ArcTerrainPatchStamp.h"
#include "Tiles/ArcTerrainSourceCache.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTerrainPatchAsset, Log, All);

// Unique, file-scoped helper namespace (project C++ style).
namespace ArcTerrainPatchAssetLocals_3b9d
{
	/** Emit the undecodable-format, sRGB-8-bit and alpha-on-single-channel validation warnings for
	 *  one texture read. The advisory two only apply to a format the decode can actually read —
	 *  TSF_BGRE8 is 8-bit AND undecodable, so ungated it would draw an sRGB warning about values
	 *  nothing ever reads. */
	void ValidateSourceTexture(const UTexture2D* Texture, EArcTerrainSourceChannel SourceChannel)
	{
		if (Texture == nullptr || !Texture->Source.IsValid())
		{
			return;
		}

		const ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();
		if (!FArcTerrainSourceCache::IsDecodableSourceFormat(SourceFormat))
		{
			UE_LOGF(LogArcTerrainPatchAsset, Warning,"UArcTerrainPatchAsset: texture '%ls' has source format %d, which the terrain source decode cannot read — it contributes nothing. Re-import as G8/G16/BGRA8/RGBA16/R16F/RGBA16F/R32F/RGBA32F (a .hdr import lands as TSF_BGRE8 and is unreadable).",
				*Texture->GetName(), static_cast<int32>(SourceFormat));
			return;
		}

		if (Texture->SRGB && FArcTerrainSourceCache::IsEightBitSourceFormat(SourceFormat))
		{
			UE_LOGF(LogArcTerrainPatchAsset, Warning,"UArcTerrainPatchAsset: texture '%ls' is 8-bit with SRGB enabled — the terrain source decode does not gamma-linearize, it reads the raw normalized bytes, but the texture was authored for gamma-corrected display, so those raw bytes may not be the linear values the height/weight field needs. Use a 16-bit (G16) or non-sRGB texture.",
				*Texture->GetName());
		}
		if (SourceChannel == EArcTerrainSourceChannel::A && FArcTerrainSourceCache::IsSingleChannelSourceFormat(SourceFormat))
		{
			UE_LOGF(LogArcTerrainPatchAsset, Warning,"UArcTerrainPatchAsset: texture '%ls' is single-channel — the terrain source decode ignores the requested channel for single-channel formats, so Alpha reads the same values as the default channel; the only cost is a duplicate cache entry for the same raster.",
				*Texture->GetName());
		}
	}
}

void UArcTerrainPatchAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	using namespace ArcTerrainPatchAssetLocals_3b9d;

	Super::PostEditChangeProperty(PropertyChangedEvent);

	ValidateSourceTexture(HeightTexture, HeightSourceChannel);

	for (const FArcTerrainPatchWeightLayer& Layer : WeightLayers)
	{
		ValidateSourceTexture(Layer.Texture, Layer.SourceChannel);
	}

	// Interactive edits are deliberately coalesced to the final event, matching stamp property edits.
	// Only loaded stamps can be reached here; unloaded World Partition consumers invalidate when they
	// are loaded through their normal serialized/build-key path.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	for (TObjectIterator<UArcTerrainPatchStamp> It; It; ++It)
	{
		UArcTerrainPatchStamp* Stamp = *It;
		if (!IsValid(Stamp)
			|| Stamp->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)
			|| Stamp->IsTemplate()
			|| !Stamp->IsRegistered())
		{
			continue;
		}

		Stamp->NotifyPatchAssetChanged(*this);
	}
}
#endif // WITH_EDITOR
