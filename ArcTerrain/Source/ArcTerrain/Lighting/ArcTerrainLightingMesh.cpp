// Copyright Lukasz Baran. All Rights Reserved.

#include "Lighting/ArcTerrainLightingMesh.h"

#include "Surface/ArcTerrainSurfaceSample.h"

namespace ArcTerrainLightingMeshFileLocal
{
	using ArcTerrainLightingMesh::FMesh;
	using ArcTerrainLightingMesh::GSkirtEdgeCount;

	struct FTriangleCoords
	{
		int32 Ax = 0;
		int32 Ay = 0;
		int32 Bx = 0;
		int32 By = 0;
		int32 Cx = 0;
		int32 Cy = 0;
	};

	FTriangleCoords ResolveTriangleCoords(int32 TriangleIndex, int32 Quads)
	{
		FTriangleCoords Coords;
		uint32 Id = static_cast<uint32>(TriangleIndex) + 2u;
		if ((Id & 1u) != 0u)
		{
			Coords.Bx = Quads;
			Coords.By = Quads;
			Coords.Cx = Quads;
		}
		else
		{
			Coords.Ax = Quads;
			Coords.Ay = Quads;
			Coords.Cy = Quads;
		}

		while ((Id >>= 1) > 1u)
		{
			const int32 MidX = (Coords.Ax + Coords.Bx) >> 1;
			const int32 MidY = (Coords.Ay + Coords.By) >> 1;
			if ((Id & 1u) != 0u)
			{
				Coords.Bx = Coords.Ax;
				Coords.By = Coords.Ay;
				Coords.Ax = Coords.Cx;
				Coords.Ay = Coords.Cy;
			}
			else
			{
				Coords.Ax = Coords.Bx;
				Coords.Ay = Coords.By;
				Coords.Bx = Coords.Cx;
				Coords.By = Coords.Cy;
			}
			Coords.Cx = MidX;
			Coords.Cy = MidY;
		}
		return Coords;
	}

	void BuildErrorPyramid(TConstArrayView<float> LatticeHeights, int32 Quads, TArray<float>& OutErrors)
	{
		const int32 Side = Quads + 1;
		OutErrors.Init(0.0f, Side * Side);

		const int32 NumTriangles = Quads * Quads * 2 - 2;
		const int32 NumParentTriangles = NumTriangles - Quads * Quads;

		for (int32 TriangleIndex = NumTriangles - 1; TriangleIndex >= 0; --TriangleIndex)
		{
			const FTriangleCoords Coords = ResolveTriangleCoords(TriangleIndex, Quads);

			const int32 MidX = (Coords.Ax + Coords.Bx) >> 1;
			const int32 MidY = (Coords.Ay + Coords.By) >> 1;
			const int32 MidIndex = MidY * Side + MidX;

			const float Chord = 0.5f * (LatticeHeights[Coords.Ay * Side + Coords.Ax]
				+ LatticeHeights[Coords.By * Side + Coords.Bx]);
			float Error = FMath::Max(OutErrors[MidIndex], FMath::Abs(Chord - LatticeHeights[MidIndex]));

			// Max, never sum: summing over-estimates the subtree and refines where nothing is wrong.
			if (TriangleIndex < NumParentTriangles)
			{
				const int32 LeftChildIndex = ((Coords.Ay + Coords.Cy) >> 1) * Side + ((Coords.Ax + Coords.Cx) >> 1);
				const int32 RightChildIndex = ((Coords.By + Coords.Cy) >> 1) * Side + ((Coords.Bx + Coords.Cx) >> 1);
				Error = FMath::Max3(Error, OutErrors[LeftChildIndex], OutErrors[RightChildIndex]);
			}

			OutErrors[MidIndex] = Error;
		}
	}

	FVector3f MakeSurfaceTangent(const FVector3f& Normal)
	{
		const FVector3f Axis(1.0f, 0.0f, 0.0f);
		FVector3f Tangent = (Axis - Normal * FVector3f::DotProduct(Normal, Axis)).GetSafeNormal();
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector3f(0.0f, 1.0f, 0.0f);
		}
		return Tangent;
	}

	void AppendVertex(FMesh& Mesh, const FVector3f& Position, const FVector3f& Normal,
		const FVector3f& Tangent, const FVector2f& UV)
	{
		Mesh.Positions.Add(Position);
		Mesh.Normals.Add(Normal);
		Mesh.Tangents.Add(Tangent);
		Mesh.Bitangents.Add(FVector3f::CrossProduct(Normal, Tangent));
		Mesh.UV0.Add(UV);
		Mesh.UV1.Add(UV);
		Mesh.UV2.Add(FVector2f::ZeroVector);
	}

	struct FExtractContext
	{
		TConstArrayView<float> LatticeHeights;
		TConstArrayView<FVector3f> LatticeNormals;
		TConstArrayView<float> Errors;
		TArray<int32> VertexIndexOf;
		int32 Quads = 0;
		float QuadSizeCm = 0.0f;
		float InvQuads = 0.0f;
		float ErrorToleranceCm = 0.0f;
		FMesh* Mesh = nullptr;
	};

	uint32 ResolveVertex(FExtractContext& Context, int32 Column, int32 Row)
	{
		const int32 LatticeIndex = Row * (Context.Quads + 1) + Column;
		if (Context.VertexIndexOf[LatticeIndex] != INDEX_NONE)
		{
			return static_cast<uint32>(Context.VertexIndexOf[LatticeIndex]);
		}

		const FVector3f Normal = Context.LatticeNormals[LatticeIndex];
		const FVector2f UV(static_cast<float>(Column) * Context.InvQuads, static_cast<float>(Row) * Context.InvQuads);
		const FVector3f Position(
			static_cast<float>(Column) * Context.QuadSizeCm,
			static_cast<float>(Row) * Context.QuadSizeCm,
			Context.LatticeHeights[LatticeIndex]);

		const int32 VertexIndex = Context.Mesh->Positions.Num();
		AppendVertex(*Context.Mesh, Position, Normal, MakeSurfaceTangent(Normal), UV);
		Context.VertexIndexOf[LatticeIndex] = VertexIndex;
		return static_cast<uint32>(VertexIndex);
	}

	void ExtractTriangle(FExtractContext& Context, int32 Ax, int32 Ay, int32 Bx, int32 By, int32 Cx, int32 Cy)
	{
		const int32 MidX = (Ax + Bx) >> 1;
		const int32 MidY = (Ay + By) >> 1;

		if (FMath::Abs(Ax - Cx) + FMath::Abs(Ay - Cy) > 1
			&& Context.Errors[MidY * (Context.Quads + 1) + MidX] > Context.ErrorToleranceCm)
		{
			ExtractTriangle(Context, Cx, Cy, Ax, Ay, MidX, MidY);
			ExtractTriangle(Context, Bx, By, Cx, Cy, MidX, MidY);
			return;
		}

		Context.Mesh->Indices.Add(ResolveVertex(Context, Ax, Ay));
		Context.Mesh->Indices.Add(ResolveVertex(Context, Bx, By));
		Context.Mesh->Indices.Add(ResolveVertex(Context, Cx, Cy));
	}

	FVector3f SkirtNormal(int32 Edge)
	{
		switch (static_cast<ArcTerrainLightingMesh::ESkirtEdge>(Edge))
		{
		case ArcTerrainLightingMesh::ESkirtEdge::NegX: return FVector3f(-1.0f, 0.0f, 0.0f);
		case ArcTerrainLightingMesh::ESkirtEdge::PosX: return FVector3f(1.0f, 0.0f, 0.0f);
		case ArcTerrainLightingMesh::ESkirtEdge::NegY: return FVector3f(0.0f, -1.0f, 0.0f);
		default: return FVector3f(0.0f, 1.0f, 0.0f);
		}
	}

	void EmitSkirt(FMesh& Mesh, const TArray<int32>& OrderedTopVertices, const FVector3f& Normal, float DepthCm)
	{
		if (OrderedTopVertices.Num() < 2 || DepthCm <= 0.0f)
		{
			return;
		}

		// A wall's basis runs along its own horizontal edge; a constant +X tangent collapses on the +-X walls.
		const FVector3f Tangent = FVector3f::CrossProduct(FVector3f(0.0f, 0.0f, -1.0f), Normal).GetSafeNormal();

		const int32 FirstNewVertex = Mesh.Positions.Num();
		for (int32 Step = 0; Step < OrderedTopVertices.Num(); ++Step)
		{
			const int32 SourceVertex = OrderedTopVertices[Step];
			const FVector3f TopPosition = Mesh.Positions[SourceVertex];
			const FVector2f UV = Mesh.UV0[SourceVertex];
			AppendVertex(Mesh, TopPosition, Normal, Tangent, UV);
			AppendVertex(Mesh, TopPosition - FVector3f(0.0f, 0.0f, DepthCm), Normal, Tangent, UV);
		}

		for (int32 Step = 0; Step + 1 < OrderedTopVertices.Num(); ++Step)
		{
			const uint32 Top0 = static_cast<uint32>(FirstNewVertex + Step * 2);
			const uint32 Bottom0 = Top0 + 1u;
			const uint32 Top1 = Top0 + 2u;
			const uint32 Bottom1 = Top0 + 3u;

			Mesh.Indices.Add(Top0);
			Mesh.Indices.Add(Top1);
			Mesh.Indices.Add(Bottom1);
			Mesh.Indices.Add(Top0);
			Mesh.Indices.Add(Bottom1);
			Mesh.Indices.Add(Bottom0);

			Mesh.Indices.Add(Top0);
			Mesh.Indices.Add(Bottom1);
			Mesh.Indices.Add(Top1);
			Mesh.Indices.Add(Top0);
			Mesh.Indices.Add(Bottom0);
			Mesh.Indices.Add(Bottom1);
		}
	}

	void EmitSkirts(FMesh& Mesh, const TArray<int32> (&OrderedEdges)[GSkirtEdgeCount], const float (&DepthCm)[GSkirtEdgeCount])
	{
		for (int32 Edge = 0; Edge < GSkirtEdgeCount; ++Edge)
		{
			EmitSkirt(Mesh, OrderedEdges[Edge], SkirtNormal(Edge), DepthCm[Edge]);
		}
	}

	void FinalizeBounds(FMesh& Mesh)
	{
		FBox3f Bounds(ForceInit);
		for (const FVector3f& Position : Mesh.Positions)
		{
			Bounds += Position;
		}
		Mesh.LocalBounds = Bounds;
	}

	void AppendOffsetMesh(FMesh& Out, const FMesh& Cell, const FVector3f& Offset)
	{
		const int32 BaseVertex = Out.Positions.Num();

		Out.Positions.Reserve(BaseVertex + Cell.Positions.Num());
		for (const FVector3f& Position : Cell.Positions)
		{
			Out.Positions.Add(Position + Offset);
		}

		Out.Normals.Append(Cell.Normals);
		Out.Tangents.Append(Cell.Tangents);
		Out.Bitangents.Append(Cell.Bitangents);
		Out.UV0.Append(Cell.UV0);
		Out.UV1.Append(Cell.UV1);
		Out.UV2.Append(Cell.UV2);

		Out.Indices.Reserve(Out.Indices.Num() + Cell.Indices.Num());
		for (const uint32 Index : Cell.Indices)
		{
			Out.Indices.Add(Index + static_cast<uint32>(BaseVertex));
		}

		if (Cell.Positions.Num() > 0)
		{
			Out.LocalBounds += Cell.LocalBounds.ShiftBy(Offset);
		}
	}

	struct FAxisTaps
	{
		int32 FirstIndex = 0;
		TArray<float> Weights;
	};

	void BuildAxisTaps(int32 SrcQuads, int32 DstQuads, int32 DstIndex, FAxisTaps& Out)
	{
		Out.Weights.Reset();
		const double Center = static_cast<double>(DstIndex * SrcQuads) / static_cast<double>(DstQuads);

		if (SrcQuads < DstQuads)
		{
			const int32 Base = FMath::Clamp(FMath::FloorToInt32(Center), -1, SrcQuads);
			const double Frac = Center - static_cast<double>(Base);
			Out.FirstIndex = Base;
			if (Frac <= 0.0)
			{
				Out.Weights.Add(1.0f);
			}
			else
			{
				Out.Weights.Add(static_cast<float>(1.0 - Frac));
				Out.Weights.Add(static_cast<float>(Frac));
			}
			return;
		}

		const double FullHalf = 0.5 * static_cast<double>(SrcQuads) / static_cast<double>(DstQuads);
		const double Half = FMath::Min3(FullHalf, Center + 1.0, static_cast<double>(SrcQuads) + 1.0 - Center);
		const double Low = Center - Half;
		const double High = Center + Half;

		const int32 FirstIndex = FMath::Clamp(FMath::FloorToInt32(Low - 0.5) + 1, -1, SrcQuads + 1);
		const int32 LastIndex = FMath::Clamp(FMath::CeilToInt32(High + 0.5) - 1, FirstIndex, SrcQuads + 1);

		Out.FirstIndex = FirstIndex;
		for (int32 Index = FirstIndex; Index <= LastIndex; ++Index)
		{
			const double TapLow = FMath::Max(static_cast<double>(Index) - 0.5, Low);
			const double TapHigh = FMath::Min(static_cast<double>(Index) + 0.5, High);
			Out.Weights.Add(static_cast<float>(FMath::Max(TapHigh - TapLow, 0.0)));
		}
	}

	void ResamplePlane(TConstArrayView<float> SrcWithApron, int32 SrcQuads, int32 DstQuads, TArray<float>& Out)
	{
		const int32 SrcSide = SrcQuads + 3;
		const int32 DstSide = DstQuads + 3;
		check(SrcQuads > 0 && DstQuads > 0);
		check(SrcWithApron.Num() == SrcSide * SrcSide);

		TArray<FAxisTaps> AxisTaps;
		AxisTaps.SetNum(DstQuads + 1);
		for (int32 DstIndex = 0; DstIndex <= DstQuads; ++DstIndex)
		{
			BuildAxisTaps(SrcQuads, DstQuads, DstIndex, AxisTaps[DstIndex]);
		}

		Out.SetNumZeroed(DstSide * DstSide);

		for (int32 DstRow = 0; DstRow <= DstQuads; ++DstRow)
		{
			const FAxisTaps& RowTaps = AxisTaps[DstRow];
			for (int32 DstColumn = 0; DstColumn <= DstQuads; ++DstColumn)
			{
				const FAxisTaps& ColumnTaps = AxisTaps[DstColumn];

				float Sum = 0.0f;
				float WeightSum = 0.0f;
				for (int32 TapY = 0; TapY < RowTaps.Weights.Num(); ++TapY)
				{
					const float WeightY = RowTaps.Weights[TapY];
					const int32 TexelRow = RowTaps.FirstIndex + TapY + 1;
					for (int32 TapX = 0; TapX < ColumnTaps.Weights.Num(); ++TapX)
					{
						const float Weight = ColumnTaps.Weights[TapX] * WeightY;
						Sum += Weight * SrcWithApron[TexelRow * SrcSide + ColumnTaps.FirstIndex + TapX + 1];
						WeightSum += Weight;
					}
				}

				Out[(DstRow + 1) * DstSide + (DstColumn + 1)] = Sum / WeightSum;
			}
		}

		for (int32 Row = 0; Row < DstSide; ++Row)
		{
			const int32 ClampedRow = FMath::Clamp(Row, 1, DstSide - 2);
			for (int32 Column = 0; Column < DstSide; ++Column)
			{
				if (Row == ClampedRow && Column > 0 && Column < DstSide - 1)
				{
					continue;
				}
				const int32 ClampedColumn = FMath::Clamp(Column, 1, DstSide - 2);
				Out[Row * DstSide + Column] = Out[ClampedRow * DstSide + ClampedColumn];
			}
		}
	}
}

namespace ArcTerrainLightingMesh
{
	double HeightfieldQuadToLocal(double CellSize)
	{
		return CellSize / static_cast<double>(GHeightfieldResolution - 1);
	}

	void BuildCellMesh(
		const FArcTerrainSurfaceCellCache& Cell,
		double CellSize,
		float ErrorToleranceCm,
		const float (&SkirtDepthCm)[GSkirtEdgeCount],
		FMesh& Out)
	{
		const int32 Quads = Cell.RenderQuads;
		check(Quads > 0 && FMath::IsPowerOfTwo(Quads));
		check(CellSize > 0.0);
		check(Cell.HeightsWithApron.Num() == (Quads + 3) * (Quads + 3));
		check(Cell.NormalsWithApron.Num() == (Quads + 3) * (Quads + 3));

		Out = FMesh();

		const int32 Side = Quads + 1;
		const float InvQuads = 1.0f / static_cast<float>(Quads);

		TArray<float> LatticeHeights;
		TArray<FVector3f> LatticeNormals;
		LatticeHeights.SetNumUninitialized(Side * Side);
		LatticeNormals.SetNumUninitialized(Side * Side);
		for (int32 Row = 0; Row < Side; ++Row)
		{
			const float FracY = static_cast<float>(Row) * InvQuads;
			for (int32 Column = 0; Column < Side; ++Column)
			{
				const float FracX = static_cast<float>(Column) * InvQuads;
				const int32 LatticeIndex = Row * Side + Column;
				LatticeHeights[LatticeIndex] = ArcTerrainSurface::BilinearApron(Cell.HeightsWithApron, Quads, FracX, FracY);
				LatticeNormals[LatticeIndex] =
					ArcTerrainSurface::BilinearApronVec(Cell.NormalsWithApron, Quads, FracX, FracY).GetSafeNormal();
			}
		}

		TArray<float> Errors;
		ArcTerrainLightingMeshFileLocal::BuildErrorPyramid(LatticeHeights, Quads, Errors);

		ArcTerrainLightingMeshFileLocal::FExtractContext Context;
		Context.LatticeHeights = LatticeHeights;
		Context.LatticeNormals = LatticeNormals;
		Context.Errors = Errors;
		Context.VertexIndexOf.Init(INDEX_NONE, Side * Side);
		Context.Quads = Quads;
		Context.QuadSizeCm = static_cast<float>(CellSize / static_cast<double>(Quads));
		Context.InvQuads = InvQuads;
		Context.ErrorToleranceCm = FMath::Max(ErrorToleranceCm, 0.0f);
		Context.Mesh = &Out;

		ArcTerrainLightingMeshFileLocal::ExtractTriangle(Context, Quads, Quads, 0, 0, 0, Quads);
		ArcTerrainLightingMeshFileLocal::ExtractTriangle(Context, 0, 0, Quads, Quads, Quads, 0);

		TArray<int32> OrderedEdges[GSkirtEdgeCount];
		for (int32 Row = Quads; Row >= 0; --Row)
		{
			const int32 VertexIndex = Context.VertexIndexOf[Row * Side];
			if (VertexIndex != INDEX_NONE)
			{
				OrderedEdges[static_cast<int32>(ESkirtEdge::NegX)].Add(VertexIndex);
			}
		}
		for (int32 Row = 0; Row <= Quads; ++Row)
		{
			const int32 VertexIndex = Context.VertexIndexOf[Row * Side + Quads];
			if (VertexIndex != INDEX_NONE)
			{
				OrderedEdges[static_cast<int32>(ESkirtEdge::PosX)].Add(VertexIndex);
			}
		}
		for (int32 Column = 0; Column <= Quads; ++Column)
		{
			const int32 VertexIndex = Context.VertexIndexOf[Column];
			if (VertexIndex != INDEX_NONE)
			{
				OrderedEdges[static_cast<int32>(ESkirtEdge::NegY)].Add(VertexIndex);
			}
		}
		for (int32 Column = Quads; Column >= 0; --Column)
		{
			const int32 VertexIndex = Context.VertexIndexOf[Quads * Side + Column];
			if (VertexIndex != INDEX_NONE)
			{
				OrderedEdges[static_cast<int32>(ESkirtEdge::PosY)].Add(VertexIndex);
			}
		}

		ArcTerrainLightingMeshFileLocal::EmitSkirts(Out, OrderedEdges, SkirtDepthCm);
		ArcTerrainLightingMeshFileLocal::FinalizeBounds(Out);
	}

	void BuildFlatCellMesh(
		float HeightCm,
		double CellSize,
		const float (&SkirtDepthCm)[GSkirtEdgeCount],
		FMesh& Out)
	{
		check(CellSize > 0.0);
		Out = FMesh();

		const float Extent = static_cast<float>(CellSize);
		const FVector3f Normal(0.0f, 0.0f, 1.0f);
		const FVector3f Tangent(1.0f, 0.0f, 0.0f);

		ArcTerrainLightingMeshFileLocal::AppendVertex(Out, FVector3f(0.0f, 0.0f, HeightCm), Normal, Tangent, FVector2f(0.0f, 0.0f));
		ArcTerrainLightingMeshFileLocal::AppendVertex(Out, FVector3f(Extent, 0.0f, HeightCm), Normal, Tangent, FVector2f(1.0f, 0.0f));
		ArcTerrainLightingMeshFileLocal::AppendVertex(Out, FVector3f(0.0f, Extent, HeightCm), Normal, Tangent, FVector2f(0.0f, 1.0f));
		ArcTerrainLightingMeshFileLocal::AppendVertex(Out, FVector3f(Extent, Extent, HeightCm), Normal, Tangent, FVector2f(1.0f, 1.0f));

		const int32 Corner00 = 0;
		const int32 Corner10 = 1;
		const int32 Corner01 = 2;
		const int32 Corner11 = 3;

		Out.Indices.Add(static_cast<uint32>(Corner11));
		Out.Indices.Add(static_cast<uint32>(Corner00));
		Out.Indices.Add(static_cast<uint32>(Corner01));
		Out.Indices.Add(static_cast<uint32>(Corner00));
		Out.Indices.Add(static_cast<uint32>(Corner11));
		Out.Indices.Add(static_cast<uint32>(Corner10));

		TArray<int32> OrderedEdges[GSkirtEdgeCount];
		OrderedEdges[static_cast<int32>(ESkirtEdge::NegX)].Add(Corner01);
		OrderedEdges[static_cast<int32>(ESkirtEdge::NegX)].Add(Corner00);
		OrderedEdges[static_cast<int32>(ESkirtEdge::PosX)].Add(Corner10);
		OrderedEdges[static_cast<int32>(ESkirtEdge::PosX)].Add(Corner11);
		OrderedEdges[static_cast<int32>(ESkirtEdge::NegY)].Add(Corner00);
		OrderedEdges[static_cast<int32>(ESkirtEdge::NegY)].Add(Corner10);
		OrderedEdges[static_cast<int32>(ESkirtEdge::PosY)].Add(Corner11);
		OrderedEdges[static_cast<int32>(ESkirtEdge::PosY)].Add(Corner01);

		ArcTerrainLightingMeshFileLocal::EmitSkirts(Out, OrderedEdges, SkirtDepthCm);
		ArcTerrainLightingMeshFileLocal::FinalizeBounds(Out);
	}

	void BuildFarBlockMesh(
		TConstArrayView<FFarBlockSource> Sources,
		int32 SourceQuads,
		double CellSize,
		FArcTerrainCellKey BlockOriginCell,
		float ErrorToleranceCm,
		float SkirtDepthCm,
		float SinkCm,
		FMesh& Out)
	{
		check(CellSize > 0.0);

		Out = FMesh();

		const float Depth = FMath::Max(SkirtDepthCm, 0.0f);
		const float CellSkirt[GSkirtEdgeCount] = {Depth, Depth, Depth, Depth};

		// Both cell builders clear their output, so a cell is built into scratch and appended.
		FMesh Scratch;
		for (const FFarBlockSource& Source : Sources)
		{
			if (Source.bFlat)
			{
				BuildFlatCellMesh(Source.FlatHeightCm, CellSize, CellSkirt, Scratch);
			}
			else
			{
				const FArcTerrainSurfaceCellCache Cell = ArcTerrainSurface::BuildCellCache(
					Source.Key, Source.HeightsWithApron, SourceQuads, CellSize);
				BuildCellMesh(Cell, CellSize, ErrorToleranceCm, CellSkirt, Scratch);
			}

			const FArcTerrainCellKey LocalCell = Source.Key - BlockOriginCell;
			const FVector3f Offset(
				static_cast<float>(static_cast<double>(LocalCell.X) * CellSize),
				static_cast<float>(static_cast<double>(LocalCell.Y) * CellSize),
				-SinkCm);

			ArcTerrainLightingMeshFileLocal::AppendOffsetMesh(Out, Scratch, Offset);
		}
	}

	void ResampleHeightPlane(TConstArrayView<float> HeightsWithApron, int32 SrcQuads, TArray<float>& OutFloats)
	{
		ArcTerrainLightingMeshFileLocal::ResamplePlane(HeightsWithApron, SrcQuads, GLightingHeightQuads, OutFloats);
	}

	void ResampleWeightPlane(TConstArrayView<uint8> BytesWithApron, int32 SrcQuads, TArray<uint8>& OutBytes)
	{
		TArray<float> SourceFloats;
		SourceFloats.SetNumUninitialized(BytesWithApron.Num());
		for (int32 Index = 0; Index < BytesWithApron.Num(); ++Index)
		{
			SourceFloats[Index] = static_cast<float>(BytesWithApron[Index]);
		}

		TArray<float> Resampled;
		ArcTerrainLightingMeshFileLocal::ResamplePlane(SourceFloats, SrcQuads, GLightingWeightQuads, Resampled);

		OutBytes.SetNumUninitialized(Resampled.Num());
		for (int32 Index = 0; Index < Resampled.Num(); ++Index)
		{
			OutBytes[Index] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Resampled[Index]), 0, 255));
		}
	}

	void EncodePackedHeight(TConstArrayView<float> HeightsWithApron, int32 Quads, TArray<uint8>& OutRG)
	{
		check(Quads > 0 && HeightsWithApron.Num() == (Quads + 3) * (Quads + 3));

		OutRG.SetNumUninitialized(GHeightfieldResolution * GHeightfieldResolution * 2);
		const float InvSpan = 1.0f / static_cast<float>(GHeightfieldResolution - 1);

		for (int32 Row = 0; Row < GHeightfieldResolution; ++Row)
		{
			const float FracY = static_cast<float>(Row) * InvSpan;
			for (int32 Column = 0; Column < GHeightfieldResolution; ++Column)
			{
				const float FracX = static_cast<float>(Column) * InvSpan;
				// Terrain space: local Z 0 is terrain Z 0, so no cell-relative bias enters the encode.
				const double HeightCm = static_cast<double>(
					ArcTerrainSurface::BilinearApron(HeightsWithApron, Quads, FracX, FracY));
				const int64 Rounded =
					FMath::RoundToInt64(32768.0 + 128.0 * (HeightCm / static_cast<double>(GHeightfieldScaleZ)));
				const int32 Packed = static_cast<int32>(FMath::Clamp<int64>(Rounded, 0, 65535));

				const int32 ByteOffset = (Row * GHeightfieldResolution + Column) * 2;
				OutRG[ByteOffset] = static_cast<uint8>(Packed >> 8);
				OutRG[ByteOffset + 1] = static_cast<uint8>(Packed & 0xFF);
			}
		}
	}
}
