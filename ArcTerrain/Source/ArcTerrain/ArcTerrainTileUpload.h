// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTileFormat.h"
#include "PixelFormat.h"
#include "RHIFwd.h"

class FRHICommandListImmediate;

/**
 * Tile payload bytes -> atlas-ready GPU staging.
 *
 * THE STAGING TEXTURE IS THE SIZE OF A CELL, NEVER OF A TILE. ExecuteFill reads every baked source from
 * its corner (SourceOrigin = zero) and RegisterSourceTexture accepts a source LARGER than the required
 * size, so a tile-sized staging texture hands every cell of that tile the tile's corner — a smoothly
 * wrong surface with nothing in the log. The cell's sub-rect is therefore cut on the CPU, row by row,
 * before anything reaches the GPU.
 *
 * The cut functions are thread-agnostic. Everything that touches the RHI is render-thread only and says
 * so with a check.
 */
namespace ArcTerrainTileUpload
{
	/** Texel origin and edge of one cell's apron-inclusive sub-rect inside a tile plane of Grid. False,
	 *  outputs untouched, when the cell lies in another tile or the two lattices disagree. */
	ARCTERRAIN_API bool CellSubRectInTile(const FArcTerrainTileGrid& Grid, FIntPoint TileKey,
		FArcTerrainCellKey CellKey, int32 CellQuads, FIntPoint& OutOrigin, int32& OutExtent);

	/** The same rect on the WEIGHT-GROUP lattice: GroupKey counts groups of GroupQuads texels, of which a
	 *  tile holds GroupsPerTileEdge per edge. A coarse tier's group spans many cells, so the cell form's
	 *  guard and offsets name the wrong lattice for it. */
	ARCTERRAIN_API bool GroupSubRectInTile(const FArcTerrainTileGrid& Grid, FIntPoint TileKey,
		FIntPoint GroupKey, int32 GroupQuads, int32 GroupsPerTileEdge, FIntPoint& OutOrigin,
		int32& OutExtent);

	/** RENDER THREAD. Pooled on (format, resolution); null for an unusable request. Creation goes on the
	 *  caller's list — the same one the fill records onto — because a texture created on the immediate
	 *  list while a parallel list is being recorded is out of order against that recording. Give the
	 *  result to an FArcTerrainStagingTextures before recording anything that reads it. */
	ARCTERRAIN_API FTextureRHIRef AcquireStaging2D(FRHICommandListImmediate& RHICmdList,
		EPixelFormat Format, int32 Res);

	/** RENDER THREAD. Pooled on (format, resolution, slices) — the slice count is part of the key because
	 *  ExecuteFill bounds its routing loop by the source's ArraySize. */
	ARCTERRAIN_API FTextureRHIRef AcquireStagingArray(FRHICommandListImmediate& RHICmdList,
		EPixelFormat Format, int32 Res, int32 Slices);

	/** RENDER THREAD. SrcRowBytes is the source pitch; the lock's DestStride almost never equals it, so
	 *  the copy runs a row at a time. */
	ARCTERRAIN_API void FillStaging2D(FRHICommandListImmediate& RHICmdList, FTextureRHIRef Texture,
		TConstArrayView<uint8> Src, int32 SrcRowBytes);

	/** RENDER THREAD. One array layer; a 2D lock cannot address a layer. */
	ARCTERRAIN_API void FillStagingSlice(FRHICommandListImmediate& RHICmdList, FTextureRHIRef Texture,
		int32 SliceIndex, TConstArrayView<uint8> Src, int32 SrcRowBytes);

	/** RENDER THREAD. Drops every pooled texture; the module's shutdown owns the call, so the refs do not
	 *  outlive RHIExit. */
	ARCTERRAIN_API void ReleaseStagingPool();
}

/**
 * Owner of pooled staging textures for the whole span in which they may still be READ.
 *
 * The atlas copy is an RDG pass: recording it only schedules the read, and the bytes are not touched
 * until GraphBuilder.Execute(). A texture handed back to the pool at recording time is therefore handed
 * straight out to the next cell of the same job, whose lock-and-unlock write lands immediately — so the
 * first cell's slice receives the second cell's heights, smoothly wrong and silent. Textures adopted
 * here go back to the pool when this object dies, so the owner is whatever outlives the execution
 * (FArcTerrainAtlasFillJob, which the render command destroys after ExecuteFill returns).
 */
class ARCTERRAIN_API FArcTerrainStagingTextures
{
public:
	FArcTerrainStagingTextures() = default;
	~FArcTerrainStagingTextures();

	FArcTerrainStagingTextures(FArcTerrainStagingTextures&& Other);
	FArcTerrainStagingTextures& operator=(FArcTerrainStagingTextures&& Other);

	FArcTerrainStagingTextures(const FArcTerrainStagingTextures&) = delete;
	FArcTerrainStagingTextures& operator=(const FArcTerrainStagingTextures&) = delete;

	void Adopt(FTextureRHIRef Texture);

	/** Returns every held texture to the pool. Off the render thread the return is enqueued onto it. */
	void ReleaseAll();

	bool IsEmpty() const { return Textures.Num() == 0; }

private:
	TArray<FTextureRHIRef> Textures;
};
