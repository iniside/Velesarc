// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTileUpload.h"

#include "RHICommandList.h"
#include "RHIResources.h"
#include "RenderingThread.h"

namespace ArcTerrainTileUploadPrivate_3d7e
{
	struct FStagingKey
	{
		EPixelFormat Format = PF_Unknown;
		int32 Res = 0;
		int32 Slices = 0;

		bool operator==(const FStagingKey& Other) const
		{
			return Format == Other.Format && Res == Other.Res && Slices == Other.Slices;
		}
	};

	uint32 GetTypeHash(const FStagingKey& Key)
	{
		return HashCombine(HashCombine(::GetTypeHash(static_cast<int32>(Key.Format)), ::GetTypeHash(Key.Res)),
			::GetTypeHash(Key.Slices));
	}

	TMap<FStagingKey, TArray<FTextureRHIRef>> GStagingPool;

	/** A staging texture is only ever a COPY SOURCE for the atlas fill, so ShaderResource is the whole
	 *  flag set; the write side goes through the RHI's lock path. Open: the unlock puts the resource
	 *  through CopyDest while the declared initial state is SRVMask, which legacy D3D12 bindings accept
	 *  but enhanced barriers and RHI validation have not been run against. */
	FTextureRHIRef CreateStaging(FRHICommandListImmediate& RHICmdList, const FStagingKey& Key)
	{
		if (Key.Slices > 1)
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2DArray(TEXT("ArcTerrain.TileStagingArray"), Key.Res, Key.Res,
					Key.Slices, Key.Format)
					.SetFlags(ETextureCreateFlags::ShaderResource)
					.SetInitialState(ERHIAccess::SRVMask);
			return RHICmdList.CreateTexture(Desc);
		}

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("ArcTerrain.TileStaging"), Key.Res, Key.Res, Key.Format)
				.SetFlags(ETextureCreateFlags::ShaderResource)
				.SetInitialState(ERHIAccess::SRVMask);
		return RHICmdList.CreateTexture(Desc);
	}

	FTextureRHIRef Acquire(FRHICommandListImmediate& RHICmdList, const FStagingKey& Key)
	{
		check(IsInRenderingThread());

		if (Key.Res <= 0 || Key.Slices <= 0 || Key.Format == PF_Unknown
			|| GPixelFormats[Key.Format].BlockBytes <= 0)
		{
			return FTextureRHIRef();
		}

		if (TArray<FTextureRHIRef>* Bucket = GStagingPool.Find(Key))
		{
			if (Bucket->Num() > 0)
			{
				return Bucket->Pop(EAllowShrinking::No);
			}
		}

		return CreateStaging(RHICmdList, Key);
	}

	FStagingKey KeyOf(const FRHITexture& Texture)
	{
		FStagingKey Key;
		Key.Format = Texture.GetFormat();
		Key.Res = Texture.GetSizeXY().X;
		Key.Slices = FMath::Max<int32>(Texture.GetDesc().ArraySize, 1);
		return Key;
	}

	void ReturnToPool(TArray<FTextureRHIRef>&& Textures)
	{
		check(IsInRenderingThread());

		for (FTextureRHIRef& Texture : Textures)
		{
			if (Texture.IsValid())
			{
				GStagingPool.FindOrAdd(KeyOf(*Texture)).Add(MoveTemp(Texture));
			}
		}
	}

	/** Common validation of a lock target: the destination edge, the tightly packed row length the source
	 *  must at least carry, and that the source holds every row. */
	bool ResolveCopyExtent(const FRHITexture& Texture, TConstArrayView<uint8> Src, int32 SrcRowBytes,
		int32& OutNumRows, int32& OutRowBytes)
	{
		const FIntPoint Size = Texture.GetSizeXY();
		const int32 BlockBytes = GPixelFormats[Texture.GetFormat()].BlockBytes;
		if (Size.X <= 0 || Size.Y <= 0 || BlockBytes <= 0)
		{
			return false;
		}

		const int32 RowBytes = Size.X * BlockBytes;
		if (SrcRowBytes < RowBytes
			|| static_cast<int64>(Src.Num()) < static_cast<int64>(SrcRowBytes) * static_cast<int64>(Size.Y))
		{
			return false;
		}

		OutNumRows = Size.Y;
		OutRowBytes = RowBytes;
		return true;
	}
}

namespace ArcTerrainTileUpload
{
	bool CellSubRectInTile(const FArcTerrainTileGrid& Grid, FIntPoint TileKey, FArcTerrainCellKey CellKey,
		int32 CellQuads, FIntPoint& OutOrigin, int32& OutExtent)
	{
		if (!Grid.IsValid() || CellQuads <= 0 || CellQuads * Grid.CellsPerTileEdge != Grid.TileQuads)
		{
			return false;
		}

		const int64 OffsetX = static_cast<int64>(CellKey.X)
			- static_cast<int64>(TileKey.X) * static_cast<int64>(Grid.CellsPerTileEdge);
		const int64 OffsetY = static_cast<int64>(CellKey.Y)
			- static_cast<int64>(TileKey.Y) * static_cast<int64>(Grid.CellsPerTileEdge);
		if (OffsetX < 0 || OffsetY < 0 || OffsetX >= Grid.CellsPerTileEdge || OffsetY >= Grid.CellsPerTileEdge)
		{
			return false;
		}

		OutOrigin = FIntPoint(static_cast<int32>(OffsetX) * CellQuads, static_cast<int32>(OffsetY) * CellQuads);
		OutExtent = CellQuads + 3;
		return true;
	}

	bool GroupSubRectInTile(const FArcTerrainTileGrid& Grid, FIntPoint TileKey, FIntPoint GroupKey,
		int32 GroupQuads, int32 GroupsPerTileEdge, FIntPoint& OutOrigin, int32& OutExtent)
	{
		if (!Grid.IsValid() || GroupQuads <= 0 || GroupsPerTileEdge <= 0
			|| GroupQuads * GroupsPerTileEdge != Grid.TileQuads)
		{
			return false;
		}

		const int64 OffsetX = static_cast<int64>(GroupKey.X)
			- static_cast<int64>(TileKey.X) * static_cast<int64>(GroupsPerTileEdge);
		const int64 OffsetY = static_cast<int64>(GroupKey.Y)
			- static_cast<int64>(TileKey.Y) * static_cast<int64>(GroupsPerTileEdge);
		if (OffsetX < 0 || OffsetY < 0 || OffsetX >= GroupsPerTileEdge || OffsetY >= GroupsPerTileEdge)
		{
			return false;
		}

		OutOrigin = FIntPoint(static_cast<int32>(OffsetX) * GroupQuads,
			static_cast<int32>(OffsetY) * GroupQuads);
		OutExtent = GroupQuads + 3;
		return true;
	}

	FTextureRHIRef AcquireStaging2D(FRHICommandListImmediate& RHICmdList, EPixelFormat Format, int32 Res)
	{
		using namespace ArcTerrainTileUploadPrivate_3d7e;

		FStagingKey Key;
		Key.Format = Format;
		Key.Res = Res;
		Key.Slices = 1;
		return Acquire(RHICmdList, Key);
	}

	FTextureRHIRef AcquireStagingArray(FRHICommandListImmediate& RHICmdList, EPixelFormat Format, int32 Res,
		int32 Slices)
	{
		using namespace ArcTerrainTileUploadPrivate_3d7e;

		FStagingKey Key;
		Key.Format = Format;
		Key.Res = Res;
		Key.Slices = Slices;
		return Acquire(RHICmdList, Key);
	}

	void FillStaging2D(FRHICommandListImmediate& RHICmdList, FTextureRHIRef Texture,
		TConstArrayView<uint8> Src, int32 SrcRowBytes)
	{
		using namespace ArcTerrainTileUploadPrivate_3d7e;

		check(IsInRenderingThread());
		if (!Texture.IsValid())
		{
			return;
		}

		int32 NumRows = 0;
		int32 RowBytes = 0;
		if (!ResolveCopyExtent(*Texture, Src, SrcRowBytes, NumRows, RowBytes))
		{
			return;
		}

		// DestStride is the driver's pitch and almost never equals RowBytes, so the copy runs a row at a
		// time rather than as one block.
		uint32 DestStride = 0;
		uint8* Dest = static_cast<uint8*>(
			RHICmdList.LockTexture2D(Texture.GetReference(), 0, RLM_WriteOnly, DestStride, false));
		if (Dest == nullptr)
		{
			return;
		}

		for (int32 Row = 0; Row < NumRows; ++Row)
		{
			FMemory::Memcpy(Dest + static_cast<int64>(Row) * static_cast<int64>(DestStride),
				Src.GetData() + static_cast<int64>(Row) * static_cast<int64>(SrcRowBytes), RowBytes);
		}

		RHICmdList.UnlockTexture2D(Texture.GetReference(), 0, false);
	}

	void FillStagingSlice(FRHICommandListImmediate& RHICmdList, FTextureRHIRef Texture, int32 SliceIndex,
		TConstArrayView<uint8> Src, int32 SrcRowBytes)
	{
		using namespace ArcTerrainTileUploadPrivate_3d7e;

		check(IsInRenderingThread());
		if (!Texture.IsValid() || SliceIndex < 0 || SliceIndex >= Texture->GetDesc().ArraySize)
		{
			return;
		}

		int32 NumRows = 0;
		int32 RowBytes = 0;
		if (!ResolveCopyExtent(*Texture, Src, SrcRowBytes, NumRows, RowBytes))
		{
			return;
		}

		uint32 DestStride = 0;
		uint8* Dest = static_cast<uint8*>(RHICmdList.LockTexture2DArray(Texture.GetReference(),
			static_cast<uint32>(SliceIndex), 0, RLM_WriteOnly, DestStride, false));
		if (Dest == nullptr)
		{
			return;
		}

		for (int32 Row = 0; Row < NumRows; ++Row)
		{
			FMemory::Memcpy(Dest + static_cast<int64>(Row) * static_cast<int64>(DestStride),
				Src.GetData() + static_cast<int64>(Row) * static_cast<int64>(SrcRowBytes), RowBytes);
		}

		RHICmdList.UnlockTexture2DArray(Texture.GetReference(), static_cast<uint32>(SliceIndex), 0, false);
	}

	void ReleaseStagingPool()
	{
		using namespace ArcTerrainTileUploadPrivate_3d7e;

		check(IsInRenderingThread());
		GStagingPool.Empty();
	}
}

FArcTerrainStagingTextures::~FArcTerrainStagingTextures()
{
	ReleaseAll();
}

FArcTerrainStagingTextures::FArcTerrainStagingTextures(FArcTerrainStagingTextures&& Other)
	: Textures(MoveTemp(Other.Textures))
{
}

FArcTerrainStagingTextures& FArcTerrainStagingTextures::operator=(FArcTerrainStagingTextures&& Other)
{
	if (this != &Other)
	{
		ReleaseAll();
		Textures = MoveTemp(Other.Textures);
	}
	return *this;
}

void FArcTerrainStagingTextures::Adopt(FTextureRHIRef Texture)
{
	if (Texture.IsValid())
	{
		Textures.Add(MoveTemp(Texture));
	}
}

void FArcTerrainStagingTextures::ReleaseAll()
{
	using namespace ArcTerrainTileUploadPrivate_3d7e;

	if (Textures.IsEmpty())
	{
		return;
	}

	if (IsInRenderingThread())
	{
		ReturnToPool(MoveTemp(Textures));
		Textures.Reset();
		return;
	}

	// A job abandoned before EnqueueFill dies on the game thread, and the pool is render-thread state.
	ENQUEUE_RENDER_COMMAND(ArcTerrainReturnStaging)(
		[Pending = MoveTemp(Textures)](FRHICommandListImmediate&) mutable
		{
			ArcTerrainTileUploadPrivate_3d7e::ReturnToPool(MoveTemp(Pending));
		});
	Textures.Reset();
}
