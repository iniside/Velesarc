// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class AArcTerrain;
class FArcTerrainAtlasTextureResource;
class UArcTerrainAtlasTexture;
struct FArcTerrainTileAtlasResources;

inline constexpr int32 ArcTerrainPublishedMaxSlots = 32;

struct ARCTERRAIN_API FArcTerrainAtlasSnapshot
{
	FArcTerrainAtlasSnapshot();

	FMatrix44f WorldToLocal = FMatrix44f::Identity;
	FMatrix44f LocalToWorld = FMatrix44f::Identity;
	FVector3f LWCTile = FVector3f::ZeroVector;
	FIntPoint OriginSector = FIntPoint::ZeroValue;
	int32 SectorSide = 0;
	float CellSizeCm = 0.0f;
	int32 SectorShift = 0;
	int32 HeightQuads = 0;
	int32 HeightSliceSide = 0;
	int32 FillSlice = 0;

	/** Slot numbers mean nothing across a layout change — a consumer drops the whole snapshot when this moves. */
	uint64 ResourceGeneration = 0;

	uint32 ValidSlotMask = 0;
	FVector4f SliceAndOriginPerSlot[ArcTerrainPublishedMaxSlots];

	bool IsValid() const;
};

namespace ArcTerrainAtlasPublish
{
	ARCTERRAIN_API bool BuildAtlasSnapshot(const AArcTerrain& Terrain, FVector CentreWorldPos,
		FArcTerrainAtlasSnapshot& OutSnapshot);

	ARCTERRAIN_API UArcTerrainAtlasTexture* AcquireHeightAtlasFacade(AArcTerrain& Terrain);

	ARCTERRAIN_API bool IsHeightAtlasFilterable();

	ARCTERRAIN_API void PublishAtlasTextures(
		const TSharedPtr<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe>& Resources,
		FArcTerrainAtlasTextureResource* HeightResource,
		TArray<FArcTerrainAtlasTextureResource*>&& WeightResources);
}
