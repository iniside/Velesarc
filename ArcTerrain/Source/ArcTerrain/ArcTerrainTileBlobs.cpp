// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTileBlobs.h"

#if WITH_EDITOR

#include "ArcTerrainChannelSet.h"
#include "ArcTerrainTileCache.h"
#include "ArcTerrainTileFormat.h"
#include "DataMaps/ArcTerrainDataMapBake.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Weights/ArcTerrainWeightBuilder.h"

namespace ArcTerrainTileBlobs_6d24
{
	constexpr int32 GHeightBlobVersion = 1;

	constexpr int32 GWeightBlobVersion = 4;

	// Rejects blobs whose bytes were encoded under a different quantization rule: the payload is the
	// mask value quantized into the tile's per-plane window, and a key that has not moved would otherwise
	// hand a stale encoding straight to the tile writer.
	constexpr int32 GDataBlobVersion = 3;

	bool IsAddressableRaster(int32 Resolution)
	{
		const int64 Expected = static_cast<int64>(Resolution) * Resolution;
		return Resolution > 0 && Expected <= TNumericLimits<int32>::Max();
	}

	bool IsAddressablePlaneSet(int32 Resolution, int32 NumChannels)
	{
		return IsAddressableRaster(Resolution) && NumChannels > 0
			&& NumChannels <= ArcTerrainTileFormat::MaxPlaneCount;
	}
}

namespace ArcTerrainTileBlobs
{
	void BuildHeightBakeResult(TArray<float>&& Heights, FArcTerrainHeightBakeResult& OutResult)
	{
		float LocalMin = TNumericLimits<float>::Max();
		float LocalMax = TNumericLimits<float>::Lowest();
		for (int32 Index = 0; Index < Heights.Num(); ++Index)
		{
			const float Height = Heights[Index];
			if (!FMath::IsFinite(Height))
			{
				OutResult.bFinite = false;
				return;
			}
			LocalMin = FMath::Min(LocalMin, Height);
			LocalMax = FMath::Max(LocalMax, Height);
		}

		OutResult.Texels = MoveTemp(Heights);
		OutResult.LocalMin = LocalMin;
		OutResult.LocalMax = LocalMax;
		OutResult.bFinite = true;
	}

	ArcTerrainTileCache::FAsyncGetPtr BeginGetCachedHeightTexels(const FIoHash& CacheKey, int32 Resolution,
		ArcTerrainTileCache::FArcTerrainAsyncGetCompletion&& Completion)
	{
		using namespace ArcTerrainTileBlobs_6d24;

		if (!IsAddressableRaster(Resolution))
		{
			return ArcTerrainTileCache::FAsyncGetPtr();
		}
		return ArcTerrainTileCache::GetAsync(CacheKey, ArcTerrainTileCache::HeightTexelsValueId(),
			TEXT("ArcTerrainHeightTexels"), MoveTemp(Completion));
	}

	bool ConsumeCachedHeightTexels(const ArcTerrainTileCache::FAsyncGetPtr& Probe, int32 Resolution,
		TArray<float>& OutTexels)
	{
		using namespace ArcTerrainTileBlobs_6d24;

		const int64 Expected = static_cast<int64>(Resolution) * Resolution;
		if (!IsAddressableRaster(Resolution))
		{
			return false;
		}

		const FSharedBuffer Blob = ArcTerrainTileCache::TakeBuffer(Probe);
		if (Blob.IsNull())
		{
			return false;
		}

		FMemoryReaderView Reader(Blob.GetView(), /*bIsPersistent =*/ true);
		int32 BlobVersion = 0;
		int32 NumTexels = 0;
		Reader << BlobVersion;
		Reader << NumTexels;
		if (Reader.IsError() || BlobVersion != GHeightBlobVersion
			|| NumTexels != static_cast<int32>(Expected))
		{
			return false;
		}

		OutTexels.SetNumUninitialized(NumTexels);
		Reader.Serialize(OutTexels.GetData(), NumTexels * static_cast<int64>(sizeof(float)));
		if (Reader.IsError())
		{
			OutTexels.Reset();
			return false;
		}
		return true;
	}

	void PutCachedHeightTexels(const FIoHash& CacheKey, TConstArrayView<float> Texels)
	{
		using namespace ArcTerrainTileBlobs_6d24;

		if (Texels.IsEmpty())
		{
			return;
		}

		FLargeMemoryWriter Writer(0, /*bIsPersistent =*/ true);
		int32 BlobVersion = GHeightBlobVersion;
		int32 NumTexels = Texels.Num();
		Writer << BlobVersion;
		Writer << NumTexels;
		Writer.Serialize(const_cast<float*>(Texels.GetData()), NumTexels * static_cast<int64>(sizeof(float)));

		ArcTerrainTileCache::Put(CacheKey, ArcTerrainTileCache::HeightTexelsValueId(),
			FSharedBuffer::MakeView(Writer.GetData(), Writer.TotalSize()), TEXT("ArcTerrainHeightTexels"));
	}

	ArcTerrainTileCache::FAsyncGetPtr BeginGetCachedWeightPlanes(const FIoHash& CacheKey, int32 Resolution,
		int32 NumChannels, ArcTerrainTileCache::FArcTerrainAsyncGetCompletion&& Completion)
	{
		using namespace ArcTerrainTileBlobs_6d24;

		if (!IsAddressablePlaneSet(Resolution, NumChannels))
		{
			return ArcTerrainTileCache::FAsyncGetPtr();
		}
		return ArcTerrainTileCache::GetAsync(CacheKey, ArcTerrainTileCache::WeightPlanesValueId(),
			TEXT("ArcTerrainWeightPlanes"), MoveTemp(Completion));
	}

	bool ConsumeCachedWeightPlanes(const ArcTerrainTileCache::FAsyncGetPtr& Probe, int32 Resolution,
		const UArcTerrainChannelSet& ChannelSet,
		TArray<ArcTerrainWeights::FArcTerrainWeightPlane>& OutPlanes)
	{
		using namespace ArcTerrainTileBlobs_6d24;

		OutPlanes.Reset();

		const int32 NumChannels = ChannelSet.Channels.Num();
		if (!IsAddressablePlaneSet(Resolution, NumChannels))
		{
			return false;
		}
		const int64 ExpectedTexels = static_cast<int64>(Resolution) * static_cast<int64>(Resolution);

		const FSharedBuffer Blob = ArcTerrainTileCache::TakeBuffer(Probe);
		if (Blob.IsNull())
		{
			return false;
		}

		FMemoryReaderView Reader(Blob.GetView(), /*bIsPersistent =*/ true);
		int32 BlobVersion = 0;
		int32 NumPlanes = 0;
		Reader << BlobVersion;
		Reader << NumPlanes;
		if (Reader.IsError() || BlobVersion != GWeightBlobVersion || NumPlanes <= 0
			|| NumPlanes > NumChannels)
		{
			return false;
		}

		TArray<uint32> ChannelHashes;
		ChannelHashes.Reserve(NumChannels);
		for (FName Channel : ChannelSet.Channels)
		{
			ChannelHashes.Add(ArcTerrain::ChannelNameHash(Channel));
		}

		OutPlanes.Reserve(NumPlanes);
		TSet<int32> SeenChannels;
		SeenChannels.Reserve(NumPlanes);
		for (int32 PlaneIndex = 0; PlaneIndex < NumPlanes; ++PlaneIndex)
		{
			uint32 PlaneId = 0;
			int32 NumTexels = 0;
			Reader << PlaneId;
			Reader << NumTexels;
			const int32 ChannelIndex = ChannelHashes.IndexOfByKey(PlaneId);
			if (Reader.IsError() || ChannelIndex == INDEX_NONE
				|| SeenChannels.Contains(ChannelIndex)
				|| NumTexels != static_cast<int32>(ExpectedTexels))
			{
				OutPlanes.Reset();
				return false;
			}
			SeenChannels.Add(ChannelIndex);

			ArcTerrainWeights::FArcTerrainWeightPlane& Plane = OutPlanes.AddDefaulted_GetRef();
			Plane.ChannelIndex = ChannelIndex;
			Plane.G8.SetNumUninitialized(NumTexels);
			Reader.Serialize(Plane.G8.GetData(), NumTexels);
			if (Reader.IsError())
			{
				OutPlanes.Reset();
				return false;
			}
		}
		return true;
	}

	void PutCachedWeightPlanes(const FIoHash& CacheKey, const UArcTerrainChannelSet& ChannelSet,
		TConstArrayView<ArcTerrainWeights::FArcTerrainWeightPlane> Planes)
	{
		using namespace ArcTerrainTileBlobs_6d24;

		if (Planes.IsEmpty())
		{
			return;
		}

		TArray<uint32> PlaneIds;
		PlaneIds.Reserve(Planes.Num());
		for (const ArcTerrainWeights::FArcTerrainWeightPlane& Plane : Planes)
		{
			if (!ChannelSet.Channels.IsValidIndex(Plane.ChannelIndex))
			{
				return;
			}
			PlaneIds.Add(ArcTerrain::ChannelNameHash(ChannelSet.Channels[Plane.ChannelIndex]));
		}

		FLargeMemoryWriter Writer(0, /*bIsPersistent =*/ true);
		int32 BlobVersion = GWeightBlobVersion;
		int32 NumPlanes = Planes.Num();
		Writer << BlobVersion;
		Writer << NumPlanes;
		for (int32 PlaneIndex = 0; PlaneIndex < Planes.Num(); ++PlaneIndex)
		{
			const ArcTerrainWeights::FArcTerrainWeightPlane& Plane = Planes[PlaneIndex];
			uint32 PlaneId = PlaneIds[PlaneIndex];
			int32 NumTexels = Plane.G8.Num();
			Writer << PlaneId;
			Writer << NumTexels;
			Writer.Serialize(const_cast<uint8*>(Plane.G8.GetData()), NumTexels);
		}

		ArcTerrainTileCache::Put(CacheKey, ArcTerrainTileCache::WeightPlanesValueId(),
			FSharedBuffer::MakeView(Writer.GetData(), Writer.TotalSize()), TEXT("ArcTerrainWeightPlanes"));
	}

	ArcTerrainTileCache::FAsyncGetPtr BeginGetCachedDataMasks(const FIoHash& CacheKey,
		ArcTerrainTileCache::FArcTerrainAsyncGetCompletion&& Completion)
	{
		return ArcTerrainTileCache::GetAsync(CacheKey, ArcTerrainTileCache::DataMasksValueId(),
			TEXT("ArcTerrainDataMasks"), MoveTemp(Completion));
	}

	bool ConsumeCachedDataMasks(const ArcTerrainTileCache::FAsyncGetPtr& Probe, FIntPoint TileKey,
		FArcTerrainDataMapCellBake& OutBake)
	{
		using namespace ArcTerrainTileBlobs_6d24;

		OutBake = FArcTerrainDataMapCellBake();
		OutBake.Key = TileKey;

		const FSharedBuffer Blob = ArcTerrainTileCache::TakeBuffer(Probe);
		if (Blob.IsNull())
		{
			return false;
		}

		FMemoryReaderView Reader(Blob.GetView(), /*bIsPersistent =*/ true);
		int32 BlobVersion = 0;
		int32 NumMasks = 0;
		Reader << BlobVersion;
		Reader << NumMasks;
		if (Reader.IsError() || BlobVersion != GDataBlobVersion || NumMasks <= 0
			|| NumMasks > static_cast<int32>(EArcTerrainDataMap::Count))
		{
			return false;
		}

		OutBake.Masks.Reserve(NumMasks);
		for (int32 MaskIndex = 0; MaskIndex < NumMasks; ++MaskIndex)
		{
			uint8 MapIndex = 0;
			int32 Quads = 0;
			int32 NumTexels = 0;
			float QuantMin = 0.0f;
			float QuantMax = 0.0f;
			int32 CoverageMinX = 0;
			int32 CoverageMinY = 0;
			int32 CoverageMaxX = 0;
			int32 CoverageMaxY = 0;
			Reader << MapIndex;
			Reader << Quads;
			Reader << NumTexels;
			Reader << QuantMin;
			Reader << QuantMax;
			Reader << CoverageMinX;
			Reader << CoverageMinY;
			Reader << CoverageMaxX;
			Reader << CoverageMaxY;

			const int64 ExpectedTexels = Quads > 0 ? static_cast<int64>(Quads + 3) * (Quads + 3) : 0;
			if (Reader.IsError() || MapIndex >= static_cast<uint8>(EArcTerrainDataMap::Count) || Quads < 0
				|| ExpectedTexels > TNumericLimits<int32>::Max()
				|| NumTexels != static_cast<int32>(ExpectedTexels))
			{
				OutBake.Masks.Reset();
				return false;
			}

			FArcTerrainDataMapCellMask& Mask = OutBake.Masks.AddDefaulted_GetRef();
			Mask.Map = static_cast<EArcTerrainDataMap>(MapIndex);
			Mask.Quads = Quads;
			Mask.QuantMin = QuantMin;
			Mask.QuantMax = QuantMax;
			Mask.CoverageRect = FIntRect(CoverageMinX, CoverageMinY, CoverageMaxX, CoverageMaxY);
			Mask.Texels.SetNumUninitialized(NumTexels);
			Reader.Serialize(Mask.Texels.GetData(), NumTexels);
			if (Reader.IsError())
			{
				OutBake.Masks.Reset();
				return false;
			}
		}
		return true;
	}

	void PutCachedDataMasks(const FIoHash& CacheKey, const FArcTerrainDataMapCellBake& Bake)
	{
		using namespace ArcTerrainTileBlobs_6d24;

		if (Bake.Masks.IsEmpty())
		{
			return;
		}

		FLargeMemoryWriter Writer(0, /*bIsPersistent =*/ true);
		int32 BlobVersion = GDataBlobVersion;
		int32 NumMasks = Bake.Masks.Num();
		Writer << BlobVersion;
		Writer << NumMasks;
		for (const FArcTerrainDataMapCellMask& Mask : Bake.Masks)
		{
			uint8 MapIndex = static_cast<uint8>(Mask.Map);
			int32 Quads = Mask.Quads;
			int32 NumTexels = Mask.Texels.Num();
			float QuantMin = Mask.QuantMin;
			float QuantMax = Mask.QuantMax;
			int32 CoverageMinX = Mask.CoverageRect.Min.X;
			int32 CoverageMinY = Mask.CoverageRect.Min.Y;
			int32 CoverageMaxX = Mask.CoverageRect.Max.X;
			int32 CoverageMaxY = Mask.CoverageRect.Max.Y;
			Writer << MapIndex;
			Writer << Quads;
			Writer << NumTexels;
			Writer << QuantMin;
			Writer << QuantMax;
			Writer << CoverageMinX;
			Writer << CoverageMinY;
			Writer << CoverageMaxX;
			Writer << CoverageMaxY;
			Writer.Serialize(const_cast<uint8*>(Mask.Texels.GetData()), NumTexels);
		}

		ArcTerrainTileCache::Put(CacheKey, ArcTerrainTileCache::DataMasksValueId(),
			FSharedBuffer::MakeView(Writer.GetData(), Writer.TotalSize()), TEXT("ArcTerrainDataMasks"));
	}
}

#endif // WITH_EDITOR
