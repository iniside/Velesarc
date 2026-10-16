// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"

namespace ArcOcean
{
	inline constexpr int32 DefaultVertsPerSide = 65;
	inline constexpr int32 DefaultRingCount = 7;
}

struct FArcOceanGridVertex
{
	int16 GridX = 0;
	int16 GridY = 0;
	float MorphWeight = 0.0f;
};

class ARCWATER_API FArcOceanGridVertexBuffer : public FVertexBuffer
{
public:
	TArray<FArcOceanGridVertex> Vertices;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual FString GetFriendlyName() const override { return TEXT("ArcOceanGridVertexBuffer"); }
};

class ARCWATER_API FArcOceanGridIndexBuffer : public FIndexBuffer
{
public:
	TArray<uint32> Indices;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual FString GetFriendlyName() const override { return TEXT("ArcOceanGridIndexBuffer"); }
};

struct FArcOceanRingMesh
{
	FArcOceanGridVertexBuffer VertexBuffer;
	FArcOceanGridIndexBuffer IndexBuffer;

	uint32 NumVertices = 0;
	uint32 NumIndices = 0;
	uint32 NumPrimitives = 0;
};

class ARCWATER_API FArcOceanRingGeometry : public FRenderResource
{
public:
	static constexpr int32 TrimRotationCoordSum = 1;

	explicit FArcOceanRingGeometry(int32 InVertsPerSide = ArcOcean::DefaultVertsPerSide);

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("FArcOceanRingGeometry"); }

	const FArcOceanRingMesh& GetFullGridMesh() const { return FullGridMesh; }
	const FArcOceanRingMesh& GetDonutMesh() const { return DonutMesh; }
	const FArcOceanRingMesh& GetTrimMesh() const { return TrimMesh; }

	int32 GetVertsPerSide() const { return VertsPerSide; }
	int32 GetHalfExtent() const { return HalfExtent; }
	int32 GetHoleMin() const { return HoleMin; }
	int32 GetHoleMax() const { return HoleMax; }

	static int32 GetTrimRotationSteps(bool bOffsetX, bool bOffsetY);

	static uint32 GetVertexStride() { return sizeof(FArcOceanGridVertex); }

private:
	int32 VertsPerSide = ArcOcean::DefaultVertsPerSide;
	int32 HalfExtent = 0;
	int32 HoleMin = 0;
	int32 HoleMax = 0;

	FArcOceanRingMesh FullGridMesh;
	FArcOceanRingMesh DonutMesh;
	FArcOceanRingMesh TrimMesh;
};
