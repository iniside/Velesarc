// Copyright Lukasz Baran. All Rights Reserved.

#include "Stamps/ArcTerrainCollarBake.h"

#if WITH_EDITOR
#include "ArcTerrainModule.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "Engine/StaticMesh.h"
#include "Generators/MarchingCubes.h"
#include "Hash/Blake3.h"
#include "Implicit/CachingMeshSDF.h"
#include "Implicit/GridInterpolant.h"
#include "IndexTypes.h"
#include "Spatial/MeshAABBTree3.h"
#include "StaticMeshLODResourcesAdapter.h"
#include "StaticMeshResources.h"

namespace ArcTerrainCollarBake_Local
{
	using FMeshSdf = UE::Geometry::TCachingMeshSDF<FStaticMeshLODResourcesMeshAdapter>;
	using FMeshSdfInterpolant = UE::Geometry::TTriLinearGridInterpolant<FMeshSdf>;

	constexpr int64 GMaxCollarSdfCells = 32000000;
	constexpr int32 GMaxSdfCellsPerDimension = 4000;
	constexpr int64 GMaxMarchingCubesCorners = 64000000;
	constexpr int32 GMaxMarchingCubesPerDimension = 4000;
	constexpr double GMaxCollarSpanCm = 1.0e6;
	constexpr double GMaxTiltDegrees = 0.1;
	constexpr double GMinScale = 1.0e-4;
	constexpr double GMaxMeshDistance = 1.0e8;

	template <typename T>
	void HashPod(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	FGuid DigestToGuid(const FBlake3Hash& Digest)
	{
		uint32 Words[4] = { 0, 0, 0, 0 };
		FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
		return FGuid(Words[0], Words[1], Words[2], Words[3]);
	}

	bool AreHeightsFinite(const FArcTerrainCollarHeightField& Field)
	{
		for (float Height : Field.Heights)
		{
			if (!FMath::IsFinite(Height))
			{
				return false;
			}
		}
		return true;
	}

	bool IsUsableCollarBounds(const UE::Geometry::FAxisAlignedBox3d& Bounds)
	{
		if (!FMath::IsFinite(Bounds.Min.X) || !FMath::IsFinite(Bounds.Min.Y) || !FMath::IsFinite(Bounds.Min.Z)
			|| !FMath::IsFinite(Bounds.Max.X) || !FMath::IsFinite(Bounds.Max.Y) || !FMath::IsFinite(Bounds.Max.Z))
		{
			return false;
		}
		if (Bounds.Width() <= 0.0 || Bounds.Height() <= 0.0 || Bounds.Depth() <= 0.0)
		{
			return false;
		}
		return FMath::Max3(Bounds.Width(), Bounds.Height(), Bounds.Depth()) <= GMaxCollarSpanCm;
	}

	const FStaticMeshLODResources* FindReadableLod0(const UStaticMesh& Mesh)
	{
		const FStaticMeshRenderData* RenderData = Mesh.GetRenderData();
		if (RenderData == nullptr || RenderData->LODResources.Num() == 0)
		{
			return nullptr;
		}
		return &RenderData->LODResources[0];
	}

	double SampleHeight(const FArcTerrainCollarHeightField& Field, double TerrainX, double TerrainY)
	{
		const double TexelSize = static_cast<double>(Field.TexelSizeCm);
		const double GridX = FMath::Clamp(TerrainX / TexelSize - static_cast<double>(Field.MinTexel.X),
			0.0, static_cast<double>(Field.Dims.X - 1));
		const double GridY = FMath::Clamp(TerrainY / TexelSize - static_cast<double>(Field.MinTexel.Y),
			0.0, static_cast<double>(Field.Dims.Y - 1));

		const int32 X0 = FMath::Clamp(FMath::FloorToInt32(GridX), 0, Field.Dims.X - 2);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt32(GridY), 0, Field.Dims.Y - 2);
		const double FracX = GridX - static_cast<double>(X0);
		const double FracY = GridY - static_cast<double>(Y0);

		const double H00 = static_cast<double>(Field.Heights[Y0 * Field.Dims.X + X0]);
		const double H10 = static_cast<double>(Field.Heights[Y0 * Field.Dims.X + X0 + 1]);
		const double H01 = static_cast<double>(Field.Heights[(Y0 + 1) * Field.Dims.X + X0]);
		const double H11 = static_cast<double>(Field.Heights[(Y0 + 1) * Field.Dims.X + X0 + 1]);

		return FMath::Lerp(FMath::Lerp(H00, H10, FracX), FMath::Lerp(H01, H11, FracX), FracY);
	}

	double SmoothMin(double A, double B, double Radius)
	{
		if (Radius <= 0.0)
		{
			return FMath::Min(A, B);
		}
		const double Alpha = FMath::Clamp(0.5 + 0.5 * (B - A) / Radius, 0.0, 1.0);
		return FMath::Lerp(B, A, Alpha) - Radius * Alpha * (1.0 - Alpha);
	}

	struct FCollarField
	{
		const FArcTerrainCollarHeightField* Height = nullptr;
		FMeshSdfInterpolant MeshSdf;
		FTransform TerrainToComponent;
		double MeshScale = 1.0;
		double RadiusCm = 30.0;
		double GradientStepCm = 1.0;

		double MeshDistance(const FVector3d& Point) const
		{
			const FVector3d Local = TerrainToComponent.TransformPosition(Point);
			const double Raw = MeshSdf.Value(Local);
			return FMath::Clamp(Raw, -GMaxMeshDistance, GMaxMeshDistance) * MeshScale;
		}

		double GroundDistance(const FVector3d& Point) const
		{
			return Point.Z - SampleHeight(*Height, Point.X, Point.Y);
		}

		double Value(const FVector3d& Point) const
		{
			return SmoothMin(MeshDistance(Point), GroundDistance(Point), RadiusCm);
		}

		double BlendGap(const FVector3d& Point) const
		{
			const double MeshValue = MeshDistance(Point);
			const double GroundValue = GroundDistance(Point);
			return FMath::Abs(
				SmoothMin(MeshValue, GroundValue, RadiusCm) - FMath::Min(MeshValue, GroundValue));
		}

		FVector3d Gradient(const FVector3d& Point) const
		{
			const double Step = GradientStepCm;
			const double DX = Value(Point + FVector3d(Step, 0.0, 0.0)) - Value(Point - FVector3d(Step, 0.0, 0.0));
			const double DY = Value(Point + FVector3d(0.0, Step, 0.0)) - Value(Point - FVector3d(0.0, Step, 0.0));
			const double DZ = Value(Point + FVector3d(0.0, 0.0, Step)) - Value(Point - FVector3d(0.0, 0.0, Step));
			return FVector3d(DX, DY, DZ) / (2.0 * Step);
		}
	};

	/** smin(0, 0, k) is -k/4, so a contact-line point sits well inside the blended solid and no cell
	 *  corner around it changes sign; a seed only propagates once it has been pushed out to the zero set. */
	bool PushSeedToIsosurface(const FCollarField& Field, const FVector3d& Start, double MarchDistance,
		FVector3d& OutSeed)
	{
		const double StartValue = Field.Value(Start);
		if (StartValue > 0.0)
		{
			return false;
		}
		if (StartValue == 0.0)
		{
			OutSeed = Start;
			return true;
		}

		const FVector3d Direction = UE::Geometry::Normalized(Field.Gradient(Start));
		if (Direction.IsNearlyZero())
		{
			return false;
		}

		constexpr int32 MarchSteps = 8;
		constexpr int32 BisectionSteps = 12;
		const double StepLength = MarchDistance / static_cast<double>(MarchSteps);

		double Inside = 0.0;
		for (int32 StepIndex = 1; StepIndex <= MarchSteps; ++StepIndex)
		{
			const double Outside = static_cast<double>(StepIndex) * StepLength;
			if (Field.Value(Start + Direction * Outside) < 0.0)
			{
				Inside = Outside;
				continue;
			}

			double Low = Inside;
			double High = Outside;
			for (int32 BisectionIndex = 0; BisectionIndex < BisectionSteps; ++BisectionIndex)
			{
				const double Middle = 0.5 * (Low + High);
				if (Field.Value(Start + Direction * Middle) < 0.0)
				{
					Low = Middle;
				}
				else
				{
					High = Middle;
				}
			}
			OutSeed = Start + Direction * (0.5 * (Low + High));
			return true;
		}

		return false;
	}

	struct FSdfGridSize
	{
		int32 NI = 0;
		int32 NJ = 0;
		int32 NK = 0;
		int64 Cells = 0;
		double LongestSpan = 0.0;
	};

	FSdfGridSize ComputeSdfGridSize(const UE::Geometry::FAxisAlignedBox3d& MeshBounds, double CellSize,
		double MaxOffsetDistance)
	{
		const double BufferWidth = FMath::Max(4.0 * CellSize, 2.0 * MaxOffsetDistance + 2.0 * CellSize);
		const FVector3d Extent = MeshBounds.Max - MeshBounds.Min;

		FSdfGridSize Size;
		Size.NI = static_cast<int32>((Extent.X + 2.0 * BufferWidth) / CellSize) + 1;
		Size.NJ = static_cast<int32>((Extent.Y + 2.0 * BufferWidth) / CellSize) + 1;
		Size.NK = static_cast<int32>((Extent.Z + 2.0 * BufferWidth) / CellSize) + 1;
		Size.Cells = static_cast<int64>(Size.NI) * static_cast<int64>(Size.NJ) * static_cast<int64>(Size.NK);
		Size.LongestSpan = FMath::Max3(Extent.X, Extent.Y, Extent.Z) + 4.0 * MaxOffsetDistance;
		return Size;
	}

	bool FitsSdfBudget(const UE::Geometry::FAxisAlignedBox3d& MeshBounds, double CellSize,
		double MaxOffsetDistance)
	{
		const FSdfGridSize Size = ComputeSdfGridSize(MeshBounds, CellSize, MaxOffsetDistance);
		return Size.Cells <= GMaxCollarSdfCells
			&& Size.NI <= GMaxSdfCellsPerDimension
			&& Size.NJ <= GMaxSdfCellsPerDimension
			&& Size.NK <= GMaxSdfCellsPerDimension
			&& Size.LongestSpan / CellSize <= static_cast<double>(GMaxSdfCellsPerDimension);
	}

	double ResolveSdfCellSize(const UE::Geometry::FAxisAlignedBox3d& MeshBounds, double RequestedCellSize,
		double MaxOffsetDistance)
	{
		if (FitsSdfBudget(MeshBounds, RequestedCellSize, MaxOffsetDistance))
		{
			return RequestedCellSize;
		}

		double High = RequestedCellSize;
		for (int32 Attempt = 0; Attempt < 24 && !FitsSdfBudget(MeshBounds, High, MaxOffsetDistance); ++Attempt)
		{
			High *= 2.0;
		}
		if (!FitsSdfBudget(MeshBounds, High, MaxOffsetDistance))
		{
			return 0.0;
		}

		double Low = RequestedCellSize;
		for (int32 Step = 0; Step < 32; ++Step)
		{
			const double Middle = 0.5 * (Low + High);
			if (FitsSdfBudget(MeshBounds, Middle, MaxOffsetDistance))
			{
				High = Middle;
			}
			else
			{
				Low = Middle;
			}
		}
		return High;
	}

	bool FitsMarchingCubesBudget(const UE::Geometry::FAxisAlignedBox3d& Bounds, double CubeSize)
	{
		if (CubeSize <= 0.0 || !FMath::IsFinite(CubeSize))
		{
			return false;
		}

		const double CountX = FMath::Floor(Bounds.Width() / CubeSize) + 1.0;
		const double CountY = FMath::Floor(Bounds.Height() / CubeSize) + 1.0;
		const double CountZ = FMath::Floor(Bounds.Depth() / CubeSize) + 1.0;
		if (FMath::Max3(CountX, CountY, CountZ) + 1.0 > static_cast<double>(GMaxMarchingCubesPerDimension))
		{
			return false;
		}
		return (CountX + 1.0) * (CountY + 1.0) * (CountZ + 1.0)
			<= static_cast<double>(GMaxMarchingCubesCorners);
	}

	double ResolveMarchingCubeSize(const UE::Geometry::FAxisAlignedBox3d& Bounds, double RequestedCubeSize)
	{
		if (FitsMarchingCubesBudget(Bounds, RequestedCubeSize))
		{
			return RequestedCubeSize;
		}

		double High = RequestedCubeSize;
		for (int32 Attempt = 0; Attempt < 64 && !FitsMarchingCubesBudget(Bounds, High); ++Attempt)
		{
			High *= 2.0;
		}
		if (!FitsMarchingCubesBudget(Bounds, High))
		{
			return 0.0;
		}

		double Low = RequestedCubeSize;
		for (int32 Step = 0; Step < 32; ++Step)
		{
			const double Middle = 0.5 * (Low + High);
			if (FitsMarchingCubesBudget(Bounds, Middle))
			{
				High = Middle;
			}
			else
			{
				Low = Middle;
			}
		}
		return High;
	}
}

FGuid ArcTerrainCollarBake::ComputeContentId(const UStaticMesh& Mesh, const FTransform& ComponentToTerrain,
	const FArcTerrainCollarHeightField& HeightField, const FArcTerrainCollarParams& Params)
{
	using namespace ArcTerrainCollarBake_Local;

	static const FGuid FormatVersion(0x7C31A0E5, 0x2D8F4A17, 0xB6C90D42, 0x51E7338A);

	if (!HeightField.IsValid())
	{
		return FGuid();
	}

	const FStaticMeshLODResources* Lod0 = FindReadableLod0(Mesh);
	if (Lod0 == nullptr)
	{
		return FGuid();
	}

	FStaticMeshLODResourcesMeshAdapter Adapter(Lod0);
	const int32 TriangleCount = Adapter.TriangleCount();
	const int32 VertexCount = Adapter.VertexCount();
	if (TriangleCount == 0 || VertexCount == 0)
	{
		return FGuid();
	}

	FBlake3 Hash;
	HashPod(Hash, FormatVersion.A);
	HashPod(Hash, FormatVersion.B);
	HashPod(Hash, FormatVersion.C);
	HashPod(Hash, FormatVersion.D);

	const FString MeshPath = Mesh.GetPathName();
	Hash.Update(*MeshPath, MeshPath.Len() * sizeof(TCHAR));
	HashPod(Hash, TriangleCount);
	HashPod(Hash, VertexCount);
	for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const FVector3f Position = FVector3f(Adapter.GetVertex(VertexIndex));
		HashPod(Hash, Position.X);
		HashPod(Hash, Position.Y);
		HashPod(Hash, Position.Z);
	}
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		const UE::Geometry::FIndex3i Triangle = Adapter.GetTriangle(TriangleIndex);
		HashPod(Hash, Triangle.A);
		HashPod(Hash, Triangle.B);
		HashPod(Hash, Triangle.C);
	}

	Hash.Update(HeightField.Heights.GetData(), HeightField.Heights.Num() * sizeof(float));
	HashPod(Hash, HeightField.MinTexel.X);
	HashPod(Hash, HeightField.MinTexel.Y);
	HashPod(Hash, HeightField.Dims.X);
	HashPod(Hash, HeightField.Dims.Y);
	HashPod(Hash, HeightField.TexelSizeCm);

	const FVector Translation = ComponentToTerrain.GetTranslation();
	const FQuat Rotation = ComponentToTerrain.GetRotation();
	const FVector Scale = ComponentToTerrain.GetScale3D();
	HashPod(Hash, Translation.X);
	HashPod(Hash, Translation.Y);
	HashPod(Hash, Translation.Z);
	HashPod(Hash, Rotation.X);
	HashPod(Hash, Rotation.Y);
	HashPod(Hash, Rotation.Z);
	HashPod(Hash, Rotation.W);
	HashPod(Hash, Scale.X);
	HashPod(Hash, Scale.Y);
	HashPod(Hash, Scale.Z);

	HashPod(Hash, Params.CollarRadiusCm);
	HashPod(Hash, Params.CollarTexelCm);
	HashPod(Hash, Params.CollarSdfCellCm);
	HashPod(Hash, Params.CollarTrimCm);
	HashPod(Hash, Params.CollarBiasCm);

	return DigestToGuid(Hash.Finalize());
}

bool ArcTerrainCollarBake::Bake(const UStaticMesh& Mesh, const FTransform& ComponentToTerrain,
	const FArcTerrainCollarHeightField& HeightField, const FArcTerrainCollarParams& Params,
	const FString& DebugName, FArcTerrainCollarMesh& OutCollar)
{
	using namespace ArcTerrainCollarBake_Local;

	OutCollar.Reset();

	if (!HeightField.IsValid())
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: the terrain height texels under '%ls' are malformed (%d x %d, %d values); no collar baked.",
			*DebugName, *Mesh.GetPathName(), HeightField.Dims.X, HeightField.Dims.Y, HeightField.Heights.Num());
		return false;
	}

	if (!AreHeightsFinite(HeightField))
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: the terrain height texels under '%ls' contain non-finite values; no collar baked.",
			*DebugName, *Mesh.GetPathName());
		return false;
	}

	if (!HeightField.bComplete)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: a stamp covering '%ls' did not resolve on the CPU, so the ground under it is not final; "
				 "no collar baked and nothing cached.",
			*DebugName, *Mesh.GetPathName());
		return false;
	}

	if (Params.CollarRadiusCm <= 0.0f || Params.CollarTexelCm <= 0.0f || Params.CollarSdfCellCm <= 0.0f)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: collar radius %.2f, cube %.2f and SDF cell %.2f must all be positive; no collar baked.",
			*DebugName, Params.CollarRadiusCm, Params.CollarTexelCm, Params.CollarSdfCellCm);
		return false;
	}

	const FRotator PoseRotation = ComponentToTerrain.Rotator();
	if (FMath::Abs(PoseRotation.Pitch) > GMaxTiltDegrees || FMath::Abs(PoseRotation.Roll) > GMaxTiltDegrees)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: pose of '%ls' is pitched %.2f and rolled %.2f against the terrain frame, which the height "
				 "field cannot be expressed in; no collar baked.",
			*DebugName, *Mesh.GetPathName(), PoseRotation.Pitch, PoseRotation.Roll);
		return false;
	}

	const FVector PoseScale = ComponentToTerrain.GetScale3D();
	const double MeshScale =
		(FMath::Abs(PoseScale.X) + FMath::Abs(PoseScale.Y) + FMath::Abs(PoseScale.Z)) / 3.0;
	if (MeshScale < GMinScale)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: pose of '%ls' is degenerate (scale %ls); no collar baked.",
			*DebugName, *Mesh.GetPathName(), *PoseScale.ToString());
		return false;
	}

	const FStaticMeshLODResources* Lod0 = FindReadableLod0(Mesh);
	if (Lod0 == nullptr)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: '%ls' has no LOD0 render data; no collar baked.",
			*DebugName, *Mesh.GetPathName());
		return false;
	}

	FStaticMeshLODResourcesMeshAdapter Adapter(Lod0);
	if (Adapter.TriangleCount() == 0 || Adapter.VertexCount() == 0)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: LOD0 of '%ls' is not CPU-readable; no collar baked.",
			*DebugName, *Mesh.GetPathName());
		return false;
	}

	const FGuid ContentId = ComputeContentId(Mesh, ComponentToTerrain, HeightField, Params);
	if (!ContentId.IsValid())
	{
		return false;
	}

	UE::Geometry::TMeshAABBTree3<FStaticMeshLODResourcesMeshAdapter> Tree(&Adapter);
	const UE::Geometry::FAxisAlignedBox3d MeshBounds = Tree.GetBoundingBox();
	if (MeshBounds.IsEmpty())
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: LOD0 of '%ls' has a degenerate box; no collar baked.",
			*DebugName, *Mesh.GetPathName());
		return false;
	}

	const double RadiusCm = static_cast<double>(Params.CollarRadiusCm);
	const double MaxOffsetLocal = RadiusCm / MeshScale;
	const double RequestedCellLocal = static_cast<double>(Params.CollarSdfCellCm) / MeshScale;
	const double CellLocal = ResolveSdfCellSize(MeshBounds, RequestedCellLocal, MaxOffsetLocal);
	if (CellLocal <= 0.0)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: the SDF of '%ls' does not fit the %lld cell budget at any cell size; no collar baked.",
			*DebugName, *Mesh.GetPathName(), GMaxCollarSdfCells);
		return false;
	}
	if (CellLocal > RequestedCellLocal)
	{
		UE_LOGF(LogArcTerrain, Log,"%ls: the SDF of '%ls' needs %.1f cm cells to fit the %lld cell budget, up from the requested "
				 "%.1f cm; the fillet is correspondingly coarser.",
			*DebugName, *Mesh.GetPathName(), CellLocal * MeshScale, GMaxCollarSdfCells, Params.CollarSdfCellCm);
	}

	FMeshSdf MeshSdf;
	MeshSdf.Mesh = &Adapter;
	MeshSdf.Spatial = &Tree;
	MeshSdf.CellSize = static_cast<float>(CellLocal);
	MeshSdf.MaxOffsetDistance = static_cast<float>(MaxOffsetLocal);
	MeshSdf.Initialize();

	FCollarField Field{ &HeightField, MeshSdf.MakeInterpolant(), ComponentToTerrain.Inverse(), MeshScale,
		RadiusCm, FMath::Max(0.1, 0.25 * static_cast<double>(Params.CollarTexelCm)) };

	const FBox FootprintBox = FBox(FVector(MeshBounds.Min), FVector(MeshBounds.Max)).TransformBy(ComponentToTerrain);

	const FBox CollarFootprint = FootprintBox.ExpandBy(RadiusCm);

	double MinCollarHeight = TNumericLimits<double>::Max();
	double MaxCollarHeight = -TNumericLimits<double>::Max();
	int64 SampledTexelCount = 0;
	const double TexelSize = static_cast<double>(HeightField.TexelSizeCm);
	for (int32 TexelY = 0; TexelY < HeightField.Dims.Y; ++TexelY)
	{
		const double TerrainY = static_cast<double>(HeightField.MinTexel.Y + TexelY) * TexelSize;
		if (TerrainY < CollarFootprint.Min.Y || TerrainY > CollarFootprint.Max.Y)
		{
			continue;
		}
		for (int32 TexelX = 0; TexelX < HeightField.Dims.X; ++TexelX)
		{
			const double TerrainX = static_cast<double>(HeightField.MinTexel.X + TexelX) * TexelSize;
			if (TerrainX < CollarFootprint.Min.X || TerrainX > CollarFootprint.Max.X)
			{
				continue;
			}
			const double Height =
				static_cast<double>(HeightField.Heights[TexelY * HeightField.Dims.X + TexelX]);
			MinCollarHeight = FMath::Min(MinCollarHeight, Height);
			MaxCollarHeight = FMath::Max(MaxCollarHeight, Height);
			++SampledTexelCount;
		}
	}
	if (SampledTexelCount == 0)
	{
		const FVector FootprintCenter = FootprintBox.GetCenter();
		const double CenterHeight = SampleHeight(HeightField, FootprintCenter.X, FootprintCenter.Y);
		MinCollarHeight = CenterHeight;
		MaxCollarHeight = CenterHeight;
	}

	const UE::Geometry::FAxisAlignedBox3d CollarBounds(
		FVector3d(CollarFootprint.Min.X, CollarFootprint.Min.Y, MinCollarHeight - RadiusCm),
		FVector3d(CollarFootprint.Max.X, CollarFootprint.Max.Y, MaxCollarHeight + RadiusCm));

	if (!IsUsableCollarBounds(CollarBounds))
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: the contact band of '%ls' resolves to %ls .. %ls, which is not a finite box with positive "
				 "sides under %.0f cm; no collar baked.",
			*DebugName, *Mesh.GetPathName(), *CollarBounds.Min.ToString(), *CollarBounds.Max.ToString(),
			GMaxCollarSpanCm);
		return false;
	}

	const double RequestedCubeSize = static_cast<double>(Params.CollarTexelCm);
	const double CubeSize = ResolveMarchingCubeSize(CollarBounds, RequestedCubeSize);
	if (CubeSize <= 0.0)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: the contact band of '%ls' spans %.0f x %.0f x %.0f cm and fits neither the %lld corner "
				 "budget nor %d cubes per axis at any cube size; no collar baked.",
			*DebugName, *Mesh.GetPathName(), CollarBounds.Width(), CollarBounds.Height(), CollarBounds.Depth(),
			GMaxMarchingCubesCorners, GMaxMarchingCubesPerDimension);
		return false;
	}
	if (CubeSize > RequestedCubeSize)
	{
		UE_LOGF(LogArcTerrain, Log,"%ls: the contact band of '%ls' spans %.0f x %.0f x %.0f cm, so the collar is meshed at %.1f cm "
				 "cubes instead of the requested %.1f cm to fit the %lld corner budget.",
			*DebugName, *Mesh.GetPathName(), CollarBounds.Width(), CollarBounds.Height(), CollarBounds.Depth(),
			CubeSize, Params.CollarTexelCm, GMaxMarchingCubesCorners);
	}

	TArray<FVector3d> Seeds;
	const int32 SeedCountX = FMath::Max(FMath::CeilToInt32(CollarBounds.Width() / CubeSize), 1);
	const int32 SeedCountY = FMath::Max(FMath::CeilToInt32(CollarBounds.Height() / CubeSize), 1);
	for (int32 SeedY = 0; SeedY <= SeedCountY; ++SeedY)
	{
		const double TerrainY = CollarBounds.Min.Y + static_cast<double>(SeedY) * CubeSize;
		for (int32 SeedX = 0; SeedX <= SeedCountX; ++SeedX)
		{
			const double TerrainX = CollarBounds.Min.X + static_cast<double>(SeedX) * CubeSize;
			const FVector3d GroundPoint(TerrainX, TerrainY, SampleHeight(HeightField, TerrainX, TerrainY));
			if (!CollarBounds.Contains(GroundPoint))
			{
				continue;
			}
			if (FMath::Abs(Field.MeshDistance(GroundPoint)) > RadiusCm)
			{
				continue;
			}

			FVector3d Seed = FVector3d::Zero();
			if (PushSeedToIsosurface(Field, GroundPoint, 2.0 * RadiusCm, Seed) && CollarBounds.Contains(Seed))
			{
				Seeds.Add(Seed);
			}
		}
	}

	if (Seeds.Num() == 0)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: no point of the ground under '%ls' lies within %.1f cm of its surface, so there is no "
				 "contact band; no collar baked.",
			*DebugName, *Mesh.GetPathName(), Params.CollarRadiusCm);
		return false;
	}

	UE::Geometry::FMarchingCubes MarchingCubes;
	MarchingCubes.Bounds = CollarBounds;
	MarchingCubes.CubeSize = CubeSize;
	MarchingCubes.IsoValue = 0.0;
	MarchingCubes.RootMode = UE::Geometry::ERootfindingModes::LerpSteps;
	MarchingCubes.RootModeSteps = 3;
	MarchingCubes.Implicit = [&Field](const FVector3d& Point) -> double
	{
		return -Field.Value(Point);
	};
	MarchingCubes.GenerateContinuation(Seeds);
	MarchingCubes.Implicit = nullptr;

	if (MarchingCubes.Triangles.Num() == 0)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: %d seeds on the contact band of '%ls' produced no triangles; no collar baked.",
			*DebugName, Seeds.Num(), *Mesh.GetPathName());
		return false;
	}

	const FTransform TerrainToComponent = ComponentToTerrain.Inverse();
	const double TrimCm = static_cast<double>(Params.CollarTrimCm);
	const double BiasCm = static_cast<double>(Params.CollarBiasCm);
	const double FootprintSpanX = FMath::Max(CollarBounds.Width(), UE_KINDA_SMALL_NUMBER);
	const double FootprintSpanY = FMath::Max(CollarBounds.Height(), UE_KINDA_SMALL_NUMBER);

	const int32 GeneratedVertexCount = MarchingCubes.Vertices.Num();
	TArray<uint8> bKeepVertex;
	bKeepVertex.SetNumZeroed(GeneratedVertexCount);
	TArray<FVector2f> VertexUVs;
	VertexUVs.SetNumZeroed(GeneratedVertexCount);

	for (int32 VertexIndex = 0; VertexIndex < GeneratedVertexCount; ++VertexIndex)
	{
		const FVector3d Point = MarchingCubes.Vertices[VertexIndex];
		bKeepVertex[VertexIndex] = Field.BlendGap(Point) >= TrimCm ? 1 : 0;

		// smin <= min, so the strip hugs the rock and the ground from outside both; the gradient is the
		// one direction that pulls it off both at once.
		const FVector3d Gradient = Field.Gradient(Point);
		const FVector3d Biased =
			Gradient.IsNearlyZero() ? Point : Point + UE::Geometry::Normalized(Gradient) * BiasCm;

		VertexUVs[VertexIndex] = FVector2f(
			static_cast<float>((Biased.X - CollarBounds.Min.X) / FootprintSpanX),
			static_cast<float>((Biased.Y - CollarBounds.Min.Y) / FootprintSpanY));
		MarchingCubes.Vertices[VertexIndex] = TerrainToComponent.TransformPosition(Biased);
	}

	TArray<UE::Geometry::FIndex3i> KeptTriangles;
	KeptTriangles.Reserve(MarchingCubes.Triangles.Num());
	for (const UE::Geometry::FIndex3i& Triangle : MarchingCubes.Triangles)
	{
		if (bKeepVertex[Triangle.A] != 0 && bKeepVertex[Triangle.B] != 0 && bKeepVertex[Triangle.C] != 0)
		{
			KeptTriangles.Add(Triangle);
		}
	}
	if (KeptTriangles.Num() == 0)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: every triangle of the collar of '%ls' sits within %.2f cm of the unblended surfaces; "
				 "no collar baked.",
			*DebugName, *Mesh.GetPathName(), Params.CollarTrimCm);
		return false;
	}

	MarchingCubes.Triangles = MoveTemp(KeptTriangles);
	MarchingCubes.TriangleUVs.SetNum(MarchingCubes.Triangles.Num());
	MarchingCubes.TriangleNormals.SetNum(MarchingCubes.Triangles.Num());
	MarchingCubes.TrianglePolygonIDs.SetNum(MarchingCubes.Triangles.Num());

	UE::Geometry::FDynamicMesh3 CollarMesh(&MarchingCubes);
	UE::Geometry::FMeshNormals::QuickComputeVertexNormals(CollarMesh);

	TArray<int32> VertexRemap;
	VertexRemap.Init(INDEX_NONE, CollarMesh.MaxVertexID());
	OutCollar.Indices.Reserve(CollarMesh.TriangleCount() * 3);
	for (int32 TriangleId : CollarMesh.TriangleIndicesItr())
	{
		const UE::Geometry::FIndex3i Triangle = CollarMesh.GetTriangle(TriangleId);
		const int32 Corners[3] = { Triangle.A, Triangle.B, Triangle.C };
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 VertexId = Corners[Corner];
			if (!VertexUVs.IsValidIndex(VertexId))
			{
				OutCollar.Reset();
				return false;
			}
			if (VertexRemap[VertexId] == INDEX_NONE)
			{
				VertexRemap[VertexId] = OutCollar.Positions.Num();
				OutCollar.Positions.Add(FVector3f(CollarMesh.GetVertex(VertexId)));
				OutCollar.Normals.Add(CollarMesh.GetVertexNormal(VertexId));
				OutCollar.UVs.Add(VertexUVs[VertexId]);
			}
			OutCollar.Indices.Add(VertexRemap[VertexId]);
		}
	}

	OutCollar.ContentId = ContentId;
	if (!OutCollar.IsValid())
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: the collar of '%ls' came out with %d positions and %d indices; no collar baked.",
			*DebugName, *Mesh.GetPathName(), OutCollar.Positions.Num(), OutCollar.Indices.Num());
		OutCollar.Reset();
		return false;
	}

	return true;
}
#endif // WITH_EDITOR
