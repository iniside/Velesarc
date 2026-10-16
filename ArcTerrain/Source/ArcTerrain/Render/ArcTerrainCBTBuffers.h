// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"

class FRDGBuilder;

/**
 * FArcTerrainCBTBuffers — the persistent GPU state of the halfedge bisector pool.
 *
 * ONE concurrent binary tree over ONE pool: its bitfield is the pool's occupancy map (bit set == live
 * bisector), its root is the live bisector count, and its decode maps a dispatch ordinal to a live
 * slot while its complement decode maps an allocation ordinal to a free one. That is the whole
 * allocator — there is no free list, and a slot released this frame cannot be handed out again until
 * the next frame's frozen heap shows it clear.
 *
 * Buffers:
 *  - Heap: 2^(PoolDepth-1) bytes (PoolDepth authored on AArcTerrain), persistent and incremental
 *    across frames.
 *  - Bisectors: one 32-byte record per slot (node id, face, retirement kind, successors).
 *  - Neighbors: three halfedge references per slot, kept apart from the record because the link pass
 *    writes only these.
 *  - NodeCounter / PoolCounters: live count, allocation cursor, pool-full and validation counters, and
 *    the diagnostics histogram (Shaders/ArcTerrainCBTCounters.ush owns the index space).
 *  - Per-face registers (live bisector count, current root slots), sized by the terrain's static face
 *    grid. Nothing samples a texture with a face id: the renderer resolves both atlas slices per leaf
 *    out of the residency rings.
 *  - VisibleBisectors: one pool slot per bisector the frame's compaction left in the drawn list, in the
 *    order the appends landed. Persistent for the same reason the heap is: the vertex factory binds it
 *    outside RDG tracking.
 *  - DrawArgs: two 5-uint DrawIndexedIndirect blocks — the whole live set and the compacted visible list.
 *
 * Not thread-safe; all access is render-thread.
 */
class ARCTERRAIN_API FArcTerrainCBTBuffers
{
public:
	/** Slots in the PoolCounters buffer. MUST equal ARC_CBT_COUNTER_NUM in
	 *  Shaders/ArcTerrainCBTCounters.ush, which names every index; a disagreement reads back garbage. */
	static constexpr int32 PoolCounterNum = 438;

	/** Byte offset of the compacted-list DrawArgs block. MUST equal ARC_CBT_DRAWARGS_VISIBLE * 4 in
	 *  Shaders/ArcTerrainCBTCounters.ush. */
	static constexpr uint32 VisibleDrawArgsOffsetBytes = 20u;

	FArcTerrainCBTBuffers() = default;
	~FArcTerrainCBTBuffers() = default;

	FArcTerrainCBTBuffers(const FArcTerrainCBTBuffers&) = delete;
	FArcTerrainCBTBuffers& operator=(const FArcTerrainCBTBuffers&) = delete;

	/** THE clamp on the authored AArcTerrain::CBTPoolDepth. Public because BeginFrame compares the
	 *  published depth against the allocated one and both must land on the same value. Upper bound is 21:
	 *  a pool-wide pass dispatches 2^Depth / 64 groups against the 65535 per-dimension limit, and 21 is
	 *  the largest depth expressible without a wrapped 2D dispatch. */
	static int32 ClampPoolDepth(int32 InPoolDepth) { return FMath::Clamp(InPoolDepth, 8, 21); }

	/**
	 * Allocate the pool for InNumFaces base-mesh faces at InPoolDepth (clamped through ClampPoolDepth).
	 * Idempotent: releases any prior buffers, so the caller must re-seed.
	 */
	void Initialize(FRHICommandListBase& RHICmdList, int32 InNumFaces, int32 InPoolDepth);

	/** Drop all GPU buffers. */
	void Release();

	bool IsInitialized() const { return HeapBuffer.IsValid(); }

	int32 GetPoolDepth() const { return PoolDepth; }
	uint32 GetPoolCapacity() const { return 1u << static_cast<uint32>(PoolDepth); }
	int32 GetHeapWordCount() const { return 1 << (PoolDepth - 3); }
	int32 GetNumFaces() const { return NumFaces; }

	// --- Per-frame registration ---------------------------------------------

	FRDGBufferRef RegisterHeap(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterBisectors(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterNeighbors(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterNodeCounter(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterPoolCounters(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterFaceLiveCount(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterFaceRoots(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterVisibleBisectors(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterMergeScanCursor(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterDrawArgs(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterLinkArgs(FRDGBuilder& GraphBuilder) const;
	FRDGBufferRef RegisterPromote(FRDGBuilder& GraphBuilder) const;

	/**
	 * Persistent read-only ByteAddressBuffer SRVs over the heap and the bisector records, bound on the
	 * mesh batch outside RDG tracking. The pooled buffers are created with ByteAddress usage, so
	 * FRDGPooledBuffer caches a raw SRV that is stable across frames like the buffer itself.
	 */
	FRHIShaderResourceView* GetHeapSRV() const { return HeapBuffer.IsValid() ? HeapBuffer->GetSRV() : nullptr; }
	FRHIShaderResourceView* GetBisectorSRV() const { return BisectorBuffer.IsValid() ? BisectorBuffer->GetSRV() : nullptr; }
	FRHIShaderResourceView* GetVisibleBisectorSRV() const { return VisibleBisectorBuffer.IsValid() ? VisibleBisectorBuffer->GetSRV() : nullptr; }

	FRHIBuffer* GetDrawArgsBuffer() const { return DrawArgsBuffer.IsValid() ? DrawArgsBuffer->GetRHI() : nullptr; }

	/** Zero the heap, stamp the depth marker and clear the per-face registers. */
	void InitializePool(FRDGBuilder& GraphBuilder);

private:
	TRefCountPtr<FRDGPooledBuffer> HeapBuffer;
	TRefCountPtr<FRDGPooledBuffer> BisectorBuffer;
	TRefCountPtr<FRDGPooledBuffer> NeighborBuffer;
	TRefCountPtr<FRDGPooledBuffer> NodeCounterBuffer;
	TRefCountPtr<FRDGPooledBuffer> PoolCounterBuffer;
	TRefCountPtr<FRDGPooledBuffer> FaceLiveCountBuffer;
	TRefCountPtr<FRDGPooledBuffer> FaceRootsBuffer;
	TRefCountPtr<FRDGPooledBuffer> VisibleBisectorBuffer;
	TRefCountPtr<FRDGPooledBuffer> MergeScanCursorBuffer;
	TRefCountPtr<FRDGPooledBuffer> DrawArgsBuffer;
	TRefCountPtr<FRDGPooledBuffer> LinkArgsBuffer;
	TRefCountPtr<FRDGPooledBuffer> PromoteBuffer;

	int32 PoolDepth = 0;
	int32 NumFaces = 0;
};
