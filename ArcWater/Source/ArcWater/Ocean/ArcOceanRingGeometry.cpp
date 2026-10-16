// Copyright Lukasz Baran. All Rights Reserved.

#include "Ocean/ArcOceanRingGeometry.h"

#include "RHIResourceUtils.h"

namespace ArcOceanRingGeometryLocal_7b19
{
	float AxisMorphRamp(int32 Coord, int32 HalfExtent, int32 HoleMin, int32 HoleMax)
	{
		if (Coord <= HoleMin)
		{
			const int32 Span = HoleMin + HalfExtent;
			return Span > 0 ? static_cast<float>(HoleMin - Coord) / static_cast<float>(Span) : 0.0f;
		}

		if (Coord >= HoleMax)
		{
			const int32 Span = HalfExtent - HoleMax;
			return Span > 0 ? static_cast<float>(Coord - HoleMax) / static_cast<float>(Span) : 0.0f;
		}

		return 0.0f;
	}

	float ComputeMorphWeight(int32 CoordX, int32 CoordY, int32 HalfExtent, int32 HoleMin, int32 HoleMax)
	{
		const float RampX = AxisMorphRamp(CoordX, HalfExtent, HoleMin, HoleMax);
		const float RampY = AxisMorphRamp(CoordY, HalfExtent, HoleMin, HoleMax);

		// Chebyshev, not Euclidean: only the max norm reaches 1 on the corners at the same time as on the
		// side midpoints, so every outer vertex is fully morphed when it meets the coarser ring.
		return FMath::Clamp(FMath::Max(RampX, RampY), 0.0f, 1.0f);
	}

	uint32 AcquireVertex(
		int32 CoordX,
		int32 CoordY,
		int32 HalfExtent,
		int32 HoleMin,
		int32 HoleMax,
		TArray<int32>& Remap,
		TArray<FArcOceanGridVertex>& Vertices)
	{
		const int32 VertsPerSide = 2 * HalfExtent + 1;
		const int32 RemapIndex = (CoordY + HalfExtent) * VertsPerSide + (CoordX + HalfExtent);

		if (Remap[RemapIndex] == INDEX_NONE)
		{
			FArcOceanGridVertex Vertex;
			Vertex.GridX = static_cast<int16>(CoordX);
			Vertex.GridY = static_cast<int16>(CoordY);
			Vertex.MorphWeight = ComputeMorphWeight(CoordX, CoordY, HalfExtent, HoleMin, HoleMax);

			Remap[RemapIndex] = Vertices.Add(Vertex);
		}

		return static_cast<uint32>(Remap[RemapIndex]);
	}

	bool IsHoleCell(int32 Cell, int32 HoleMin, int32 HoleMax)
	{
		return Cell >= HoleMin && Cell < HoleMax;
	}

	enum class EMeshShape : uint8
	{
		FullGrid,
		Donut,
		Trim
	};

	bool IsCellIncluded(EMeshShape Shape, int32 CellX, int32 CellY, int32 HoleMin, int32 HoleMax)
	{
		const bool bInHole = IsHoleCell(CellX, HoleMin, HoleMax) && IsHoleCell(CellY, HoleMin, HoleMax);

		switch (Shape)
		{
		case EMeshShape::FullGrid:
			return true;
		case EMeshShape::Donut:
			return !bInHole;
		case EMeshShape::Trim:
			return bInHole && (CellX == HoleMax - 1 || CellY == HoleMax - 1);
		default:
			return false;
		}
	}

	void BuildMesh(FArcOceanRingMesh& OutMesh, EMeshShape Shape, int32 HalfExtent, int32 HoleMin, int32 HoleMax)
	{
		const int32 VertsPerSide = 2 * HalfExtent + 1;

		TArray<int32> Remap;
		Remap.Init(INDEX_NONE, VertsPerSide * VertsPerSide);

		TArray<FArcOceanGridVertex>& Vertices = OutMesh.VertexBuffer.Vertices;
		TArray<uint32>& Indices = OutMesh.IndexBuffer.Indices;
		Vertices.Reset();
		Indices.Reset();

		for (int32 CellY = -HalfExtent; CellY < HalfExtent; ++CellY)
		{
			for (int32 CellX = -HalfExtent; CellX < HalfExtent; ++CellX)
			{
				if (!IsCellIncluded(Shape, CellX, CellY, HoleMin, HoleMax))
				{
					continue;
				}

				const uint32 Index00 = AcquireVertex(CellX, CellY, HalfExtent, HoleMin, HoleMax, Remap, Vertices);
				const uint32 Index10 = AcquireVertex(CellX + 1, CellY, HalfExtent, HoleMin, HoleMax, Remap, Vertices);
				const uint32 Index01 = AcquireVertex(CellX, CellY + 1, HalfExtent, HoleMin, HoleMax, Remap, Vertices);
				const uint32 Index11 = AcquireVertex(CellX + 1, CellY + 1, HalfExtent, HoleMin, HoleMax, Remap, Vertices);

				Indices.Add(Index11);
				Indices.Add(Index10);
				Indices.Add(Index00);
				Indices.Add(Index00);
				Indices.Add(Index01);
				Indices.Add(Index11);
			}
		}

		OutMesh.NumVertices = static_cast<uint32>(Vertices.Num());
		OutMesh.NumIndices = static_cast<uint32>(Indices.Num());
		OutMesh.NumPrimitives = OutMesh.NumIndices / 3u;
	}
}

void FArcOceanGridVertexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	if (Vertices.Num() == 0)
	{
		return;
	}

	VertexBufferRHI = UE::RHIResourceUtils::CreateVertexBufferFromArray(
		RHICmdList,
		TEXT("ArcOceanGridVertexBuffer"),
		EBufferUsageFlags::Static,
		MakeConstArrayView(Vertices));
}

void FArcOceanGridIndexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	if (Indices.Num() == 0)
	{
		return;
	}

	IndexBufferRHI = UE::RHIResourceUtils::CreateIndexBufferFromArray(
		RHICmdList,
		TEXT("ArcOceanGridIndexBuffer"),
		EBufferUsageFlags::Static,
		MakeConstArrayView(Indices));
}

FArcOceanRingGeometry::FArcOceanRingGeometry(int32 InVertsPerSide)
{
	VertsPerSide = FMath::Max(9, ((InVertsPerSide - 1) / 4) * 4 + 1);
	HalfExtent = (VertsPerSide - 1) / 2;
	HoleMin = -HalfExtent / 2;
	HoleMax = HalfExtent / 2 + 1;

	check(HoleMin + HoleMax == TrimRotationCoordSum);

	using namespace ArcOceanRingGeometryLocal_7b19;
	BuildMesh(FullGridMesh, EMeshShape::FullGrid, HalfExtent, HoleMin, HoleMax);
	BuildMesh(DonutMesh, EMeshShape::Donut, HalfExtent, HoleMin, HoleMax);
	BuildMesh(TrimMesh, EMeshShape::Trim, HalfExtent, HoleMin, HoleMax);
}

void FArcOceanRingGeometry::InitRHI(FRHICommandListBase& RHICmdList)
{
	FullGridMesh.VertexBuffer.InitResource(RHICmdList);
	FullGridMesh.IndexBuffer.InitResource(RHICmdList);
	DonutMesh.VertexBuffer.InitResource(RHICmdList);
	DonutMesh.IndexBuffer.InitResource(RHICmdList);
	TrimMesh.VertexBuffer.InitResource(RHICmdList);
	TrimMesh.IndexBuffer.InitResource(RHICmdList);
}

void FArcOceanRingGeometry::ReleaseRHI()
{
	TrimMesh.IndexBuffer.ReleaseResource();
	TrimMesh.VertexBuffer.ReleaseResource();
	DonutMesh.IndexBuffer.ReleaseResource();
	DonutMesh.VertexBuffer.ReleaseResource();
	FullGridMesh.IndexBuffer.ReleaseResource();
	FullGridMesh.VertexBuffer.ReleaseResource();
}

int32 FArcOceanRingGeometry::GetTrimRotationSteps(bool bOffsetX, bool bOffsetY)
{
	if (!bOffsetX)
	{
		return bOffsetY ? 1 : 0;
	}

	return bOffsetY ? 2 : 3;
}
