// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainCBTBuffers.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

namespace ArcTerrainCBTBuffersPrivate_4f21
{
	/** Must match ARC_CBT_SEED_THREADS in ArcTerrainCBTSeed.usf. */
	constexpr int32 SeedThreadsPerGroup = 64;

	/** Bytes per bisector record; must match ARC_BISECTOR_STRIDE. */
	constexpr uint32 BisectorStrideBytes = 32u;

	/** Bytes per neighbour record (three halfedge references); must match ARC_NEIGHBOR_STRIDE. */
	constexpr uint32 NeighborStrideBytes = 12u;

}

/** Zero the occupancy heap, stamp libcbt's depth marker and clear the per-face registers. */
class FArcTerrainCBTPoolInitCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainCBTPoolInitCS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainCBTPoolInitCS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CBTHeap)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PoolCounters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, NodeCounter)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FaceLiveCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FaceRoots)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, MergeScanCursor)
		SHADER_PARAMETER(uint32, PoolDepth)
		SHADER_PARAMETER(uint32, HeapWordCount)
		SHADER_PARAMETER(uint32, NumFaces)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_CBT_SEED_THREADS"), ArcTerrainCBTBuffersPrivate_4f21::SeedThreadsPerGroup);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainCBTPoolInitCS,
	"/Plugin/ArcTerrain/ArcTerrainCBTSeed.usf", "PoolInitCS", SF_Compute);

void FArcTerrainCBTBuffers::Initialize(FRHICommandListBase&, int32 InNumFaces, int32 InPoolDepth)
{
	using namespace ArcTerrainCBTBuffersPrivate_4f21;

	Release();

	NumFaces = FMath::Max(InNumFaces, 1);
	PoolDepth = ClampPoolDepth(InPoolDepth);

	const uint64 PoolCapacity = 1ull << static_cast<uint64>(PoolDepth);
	const uint64 HeapBytes = 1ull << static_cast<uint64>(PoolDepth - 1);

	HeapBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateByteAddressDesc(static_cast<uint32>(HeapBytes)), TEXT("ArcTerrainCBT.Heap"));
	BisectorBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateByteAddressDesc(static_cast<uint32>(PoolCapacity * BisectorStrideBytes)),
		TEXT("ArcTerrainCBT.Bisectors"));
	NeighborBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateByteAddressDesc(static_cast<uint32>(PoolCapacity * NeighborStrideBytes)),
		TEXT("ArcTerrainCBT.Neighbors"));
	NodeCounterBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 4), TEXT("ArcTerrainCBT.NodeCounter"));
	PoolCounterBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), PoolCounterNum), TEXT("ArcTerrainCBT.PoolCounters"));
	FaceLiveCountBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumFaces), TEXT("ArcTerrainCBT.FaceLiveCount"));
	FaceRootsBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumFaces * 2), TEXT("ArcTerrainCBT.FaceRoots"));
	VisibleBisectorBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateByteAddressDesc(static_cast<uint32>(PoolCapacity * sizeof(uint32))),
		TEXT("ArcTerrainCBT.VisibleBisectors"));
	MergeScanCursorBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("ArcTerrainCBT.MergeScanCursor"));
	DrawArgsBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateIndirectDesc(10u), TEXT("ArcTerrainCBT.DrawArgs"));
	LinkArgsBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateIndirectDesc(3u), TEXT("ArcTerrainCBT.LinkArgs"));
	PromoteBuffer = AllocatePooledBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), static_cast<int32>(PoolCapacity)),
		TEXT("ArcTerrainCBT.Promote"));
}

void FArcTerrainCBTBuffers::Release()
{
	HeapBuffer.SafeRelease();
	BisectorBuffer.SafeRelease();
	NeighborBuffer.SafeRelease();
	NodeCounterBuffer.SafeRelease();
	PoolCounterBuffer.SafeRelease();
	FaceLiveCountBuffer.SafeRelease();
	FaceRootsBuffer.SafeRelease();
	VisibleBisectorBuffer.SafeRelease();
	MergeScanCursorBuffer.SafeRelease();
	DrawArgsBuffer.SafeRelease();
	LinkArgsBuffer.SafeRelease();
	PromoteBuffer.SafeRelease();

	PoolDepth = 0;
	NumFaces = 0;
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterHeap(FRDGBuilder& GraphBuilder) const
{
	check(HeapBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(HeapBuffer, TEXT("ArcTerrainCBT.Heap"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterBisectors(FRDGBuilder& GraphBuilder) const
{
	check(BisectorBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(BisectorBuffer, TEXT("ArcTerrainCBT.Bisectors"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterNeighbors(FRDGBuilder& GraphBuilder) const
{
	check(NeighborBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(NeighborBuffer, TEXT("ArcTerrainCBT.Neighbors"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterNodeCounter(FRDGBuilder& GraphBuilder) const
{
	check(NodeCounterBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(NodeCounterBuffer, TEXT("ArcTerrainCBT.NodeCounter"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterPoolCounters(FRDGBuilder& GraphBuilder) const
{
	check(PoolCounterBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(PoolCounterBuffer, TEXT("ArcTerrainCBT.PoolCounters"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterFaceLiveCount(FRDGBuilder& GraphBuilder) const
{
	check(FaceLiveCountBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(FaceLiveCountBuffer, TEXT("ArcTerrainCBT.FaceLiveCount"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterFaceRoots(FRDGBuilder& GraphBuilder) const
{
	check(FaceRootsBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(FaceRootsBuffer, TEXT("ArcTerrainCBT.FaceRoots"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterVisibleBisectors(FRDGBuilder& GraphBuilder) const
{
	check(VisibleBisectorBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(VisibleBisectorBuffer, TEXT("ArcTerrainCBT.VisibleBisectors"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterMergeScanCursor(FRDGBuilder& GraphBuilder) const
{
	check(MergeScanCursorBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(MergeScanCursorBuffer, TEXT("ArcTerrainCBT.MergeScanCursor"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterDrawArgs(FRDGBuilder& GraphBuilder) const
{
	check(DrawArgsBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(DrawArgsBuffer, TEXT("ArcTerrainCBT.DrawArgs"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterLinkArgs(FRDGBuilder& GraphBuilder) const
{
	check(LinkArgsBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(LinkArgsBuffer, TEXT("ArcTerrainCBT.LinkArgs"));
}

FRDGBufferRef FArcTerrainCBTBuffers::RegisterPromote(FRDGBuilder& GraphBuilder) const
{
	check(PromoteBuffer.IsValid());
	return GraphBuilder.RegisterExternalBuffer(PromoteBuffer, TEXT("ArcTerrainCBT.Promote"));
}

void FArcTerrainCBTBuffers::InitializePool(FRDGBuilder& GraphBuilder)
{
	using namespace ArcTerrainCBTBuffersPrivate_4f21;

	check(IsInitialized());

	FArcTerrainCBTPoolInitCS::FParameters* Parameters =
		GraphBuilder.AllocParameters<FArcTerrainCBTPoolInitCS::FParameters>();
	Parameters->CBTHeap = GraphBuilder.CreateUAV(RegisterHeap(GraphBuilder));
	Parameters->PoolCounters = GraphBuilder.CreateUAV(
		FRDGBufferUAVDesc(RegisterPoolCounters(GraphBuilder), PF_R32_UINT));
	Parameters->NodeCounter = GraphBuilder.CreateUAV(
		FRDGBufferUAVDesc(RegisterNodeCounter(GraphBuilder), PF_R32_UINT));
	Parameters->FaceLiveCount = GraphBuilder.CreateUAV(
		FRDGBufferUAVDesc(RegisterFaceLiveCount(GraphBuilder), PF_R32_UINT));
	Parameters->FaceRoots = GraphBuilder.CreateUAV(
		FRDGBufferUAVDesc(RegisterFaceRoots(GraphBuilder), PF_R32_UINT));
	Parameters->MergeScanCursor = GraphBuilder.CreateUAV(
		FRDGBufferUAVDesc(RegisterMergeScanCursor(GraphBuilder), PF_R32_UINT));
	Parameters->PoolDepth = static_cast<uint32>(PoolDepth);
	Parameters->HeapWordCount = static_cast<uint32>(GetHeapWordCount());
	Parameters->NumFaces = static_cast<uint32>(NumFaces);

	const int32 ThreadCount = FMath::Max(GetHeapWordCount(), NumFaces);
	TShaderMapRef<FArcTerrainCBTPoolInitCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcTerrainCBT.PoolInit"),
		ComputeShader, Parameters, FComputeShaderUtils::GetGroupCount(ThreadCount, SeedThreadsPerGroup));
}
