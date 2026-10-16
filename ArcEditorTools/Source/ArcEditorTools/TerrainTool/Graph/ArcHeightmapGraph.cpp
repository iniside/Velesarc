// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/ArcHeightmapGraph.h"

#include "Hash/Blake3.h"
#include "Serialization/MemoryWriter.h"
#include "TerrainTool/Graph/ArcHeightmapNodes.h"
#include "UObject/UnrealType.h"

namespace ArcHeightmapGraph
{
    void FromField(const UE::Geometry::FSampledScalarField2f& In, FArcBakedHeightField& Out)
    {
        Out.Dims = FIntPoint(In.GridValues.Width(), In.GridValues.Height());
        const TArray64<float>& Src = In.GridValues.GridValues();
        Out.Values.SetNumUninitialized(Src.Num());
        for (int64 Idx = 0; Idx < Src.Num(); ++Idx)
        {
            Out.Values[Idx] = Src[Idx];
        }
    }

    void ToField(const FArcBakedHeightField& Baked, UE::Geometry::FSampledScalarField2f& Out)
    {
        check(Baked.Dims.X >= 2 && Baked.Dims.Y >= 2);
        Out.Resize(static_cast<int64>(Baked.Dims.X), static_cast<int64>(Baked.Dims.Y), 0.f);
        Out.GridOrigin = FVector2f(-0.5f, -0.5f);
        Out.CellDimensions = FVector2f(
            1.f / static_cast<float>(Baked.Dims.X - 1),
            1.f / static_cast<float>(Baked.Dims.Y - 1));
        TArray64<float>& Dst = Out.GridValues.GridValues();
        const int64 N = FMath::Min(static_cast<int64>(Baked.Values.Num()), Dst.Num());
        for (int64 Idx = 0; Idx < N; ++Idx)
        {
            Dst[Idx] = Baked.Values[Idx];
        }
    }
}

FIoHash UArcHeightmapGraph::ComputeContentHash() const
{
	// Deterministic: serialize each node's struct path name + UPROPERTY values (tagged serialization,
	// no raw struct memcpy), then Blake3 over the byte buffer. Tagged serialization skips padding and
	// transient/editor-only members, giving a stable content hash across builds.
	TArray<uint8> Buffer;
	FMemoryWriter Writer(Buffer, /*bIsPersistent*/ true);

	for (const FInstancedStruct& Node : Nodes)
	{
		const UScriptStruct* ScriptStruct = Node.GetScriptStruct();
		if (ScriptStruct == nullptr)
		{
			continue;
		}

		// Fold the concrete type into the hash so a type change alters the result.
		FString TypePath = ScriptStruct->GetPathName();
		Writer << TypePath;

		// SerializeItem walks the UPROPERTY graph; const_cast is required by the non-const signature
		// but the data is only read here.
		const_cast<UScriptStruct*>(ScriptStruct)->SerializeItem(
			Writer, const_cast<uint8*>(Node.GetMemory()), /*Defaults*/ nullptr);
	}

	const FBlake3Hash Hash = FBlake3::HashBuffer(Buffer.GetData(), Buffer.Num());
	return FIoHash(Hash);
}

#if WITH_EDITOR
void UArcHeightmapGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Assign a stable NodeId to any node that lacks one. This is the ONLY place NodeId is populated.
	for (FInstancedStruct& Node : Nodes)
	{
		FArcHeightmapNode* NodePtr = Node.GetMutablePtr<FArcHeightmapNode>();
		if (NodePtr != nullptr && !NodePtr->NodeId.IsValid())
		{
			NodePtr->NodeId = FGuid::NewGuid();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
