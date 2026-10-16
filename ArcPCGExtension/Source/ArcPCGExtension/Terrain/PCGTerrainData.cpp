// Copyright Lukasz Baran. All Rights Reserved.

#include "Terrain/PCGTerrainData.h"

#include "PCGContext.h"
#include "Data/PCGPointArrayData.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGSurfaceSampler.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "Serialization/ArchiveCrc32.h"

namespace ArcPCGTerrainDataFileLocal
{
	// Terrain-local XY extent of the cache's cell bounds, in cm.
	void GetLocalExtentCm(const FArcTerrainSurfaceCache& Cache, double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY)
	{
		OutMinX = static_cast<double>(Cache.CellBounds.Min.X) * Cache.CellSize;
		OutMinY = static_cast<double>(Cache.CellBounds.Min.Y) * Cache.CellSize;
		OutMaxX = static_cast<double>(Cache.CellBounds.Max.X) * Cache.CellSize;
		OutMaxY = static_cast<double>(Cache.CellBounds.Max.Y) * Cache.CellSize;
	}
}

void UPCGTerrainData::Initialize(TSharedPtr<const FArcTerrainSurfaceCache> InCache, FPCGTerrainDataProps InProps)
{
	Cache = MoveTemp(InCache);
	Props = InProps;

	if (!Cache.IsValid())
	{
		return;
	}

	Transform = Cache->TerrainTransform;

	double MinX = 0.0;
	double MinY = 0.0;
	double MaxX = 0.0;
	double MaxY = 0.0;
	ArcPCGTerrainDataFileLocal::GetLocalExtentCm(*Cache, MinX, MinY, MaxX, MaxY);

	// Terrain-local Z range from the cached height tiles (cheap one-time scan; heights are local cm).
	double MinZ = 0.0;
	double MaxZ = 0.0;
	bool bHasHeights = false;
	for (const FArcTerrainSurfaceCellCache& Cell : Cache->Cells)
	{
		for (const float Height : Cell.HeightsWithApron)
		{
			MinZ = bHasHeights ? FMath::Min(MinZ, static_cast<double>(Height)) : static_cast<double>(Height);
			MaxZ = bHasHeights ? FMath::Max(MaxZ, static_cast<double>(Height)) : static_cast<double>(Height);
			bHasHeights = true;
		}
	}

	LocalBounds = FBox(FVector(MinX, MinY, MinZ), FVector(MaxX, MaxY, MaxZ));
}

void UPCGTerrainData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	if (!Cache.IsValid())
	{
		// No stable identity without a cache — fall back to the per-object UID like the base data types.
		AddUIDToCrc(Ar);
		return;
	}

	FString ClassName = StaticClass()->GetPathName();
	Ar << ClassName;

	// Stable, content-derived identity: fold of every cell's baked height payload + geometry.
	FIoHash ContentHash = Cache->ContentHash;
	Ar << ContentHash;
}

FBox UPCGTerrainData::GetBounds() const
{
	return Cache.IsValid() ? Cache->WorldBounds : FBox(EForceInit::ForceInit);
}

FBox UPCGTerrainData::GetStrictBounds() const
{
	return GetBounds();
}

FVector UPCGTerrainData::GetNormal() const
{
	const FTransform& TerrainTransform = Cache.IsValid() ? Cache->TerrainTransform : FTransform::Identity;
	return TerrainTransform.TransformVectorNoScale(FVector::UnitZ()).GetSafeNormal();
}

bool UPCGTerrainData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	if (Cache.IsValid())
	{
		const FVector Local = Cache->TerrainTransform.InverseTransformPosition(InTransform.GetLocation());

		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;
		ArcPCGTerrainDataFileLocal::GetLocalExtentCm(*Cache, MinX, MinY, MaxX, MaxY);

		if (Local.X >= MinX && Local.X <= MaxX && Local.Y >= MinY && Local.Y <= MaxY)
		{
			// Inside the terrain XY extent: delegate to projection with identity params (density 1).
			return ProjectPoint(InTransform, InBounds, FPCGProjectionParams(), OutPoint, OutMetadata);
		}
	}

	OutPoint.Density = 0.0f;
	return false;
}

bool UPCGTerrainData::ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	if (!Cache.IsValid() || Cache->CellSize <= 0.0)
	{
		return false;
	}

	const FVector Local = Cache->TerrainTransform.InverseTransformPosition(InTransform.GetLocation());

	double CellFX = Local.X / Cache->CellSize;
	double CellFY = Local.Y / Cache->CellSize;

	// Reject outside the terrain XY extent, then clamp so the far max edge is owned by the last
	// cell (CellBounds is max-exclusive; without the clamp a point exactly on the far edge floors
	// to CellBounds.Max and is lost).
	const double MinCellX = static_cast<double>(Cache->CellBounds.Min.X);
	const double MinCellY = static_cast<double>(Cache->CellBounds.Min.Y);
	const double MaxCellX = static_cast<double>(Cache->CellBounds.Max.X);
	const double MaxCellY = static_cast<double>(Cache->CellBounds.Max.Y);

	if (CellFX < MinCellX || CellFX > MaxCellX || CellFY < MinCellY || CellFY > MaxCellY)
	{
		return false;
	}

	CellFX = FMath::Clamp(CellFX, MinCellX, MaxCellX - UE_KINDA_SMALL_NUMBER);
	CellFY = FMath::Clamp(CellFY, MinCellY, MaxCellY - UE_KINDA_SMALL_NUMBER);

	const FIntPoint Key(FMath::FloorToInt32(CellFX), FMath::FloorToInt32(CellFY));
	if (Key.X < Cache->CellBounds.Min.X || Key.X >= Cache->CellBounds.Max.X
		|| Key.Y < Cache->CellBounds.Min.Y || Key.Y >= Cache->CellBounds.Max.Y)
	{
		return false;
	}

	const int32* IndexPtr = Cache->KeyToIndex.Find(Key);
	if (!IndexPtr)
	{
		// Cell inside bounds and uncached: its registered section is unreadable. An unshaped cell is
		// cached flat instead, so an absent key means a broken payload, never absent ground.
		return false;
	}

	const FArcTerrainSurfaceCellCache& Cell = Cache->Cells[*IndexPtr];
	const float FracX = static_cast<float>(CellFX - static_cast<double>(Key.X));
	const float FracY = static_cast<float>(CellFY - static_cast<double>(Key.Y));

	// Height + normal from the dense tile. MPA caveat: rendered MPA cell borders use a coarse
	// welded ring instead; sampling the dense tile everywhere deviates from the rendered mesh by
	// at most the tessellation error there (deliberate — see FArcTerrainSurfaceCellCache).
	const float HeightCm = ArcTerrainSurface::BilinearApron(Cell.HeightsWithApron, Cell.RenderQuads, FracX, FracY);
	const FVector3f LocalNormal3f = ArcTerrainSurface::BilinearApronVec(Cell.NormalsWithApron, Cell.RenderQuads, FracX, FracY);

	const FVector LocalPos(Local.X, Local.Y, static_cast<double>(HeightCm));
	const FVector WorldPos = Cache->TerrainTransform.TransformPosition(LocalPos);
	const FVector WorldNormal = Cache->TerrainTransform.TransformVectorNoScale(FVector(LocalNormal3f)).GetSafeNormal();

	// Tangent basis from the world normal (same construction as FPCGLandscapeCacheEntry).
	const FVector TangentX = FVector(WorldNormal.Z, 0.0, -WorldNormal.X).GetSafeNormal();
	const FVector TangentY = (WorldNormal ^ TangentX).GetSafeNormal();
	const FTransform SurfaceTransform(TangentX, TangentY, WorldNormal, WorldPos);

	const int32 Seed = PCGHelpers::ComputeSeedFromPosition(WorldPos);
	OutPoint = FPCGPoint(SurfaceTransform, /*Density=*/1.0f, Seed);
	if (InBounds.IsValid)
	{
		OutPoint.SetLocalBounds(InBounds);
	}

	// Respect projection settings (mirrors UPCGLandscapeData::ProjectPoint).
	if (!InParams.bProjectPositions)
	{
		OutPoint.Transform.SetLocation(InTransform.GetLocation());
	}

	if (!InParams.bProjectRotations)
	{
		OutPoint.Transform.SetRotation(InTransform.GetRotation());
	}
	else
	{
		// Take the surface rotation, but respect the initial point yaw (don't spin points around Z).
		FVector RotVector = InTransform.GetRotation().ToRotationVector();
		RotVector.X = RotVector.Y = 0.0;
		OutPoint.Transform.SetRotation(OutPoint.Transform.GetRotation() * FQuat::MakeFromRotationVector(RotVector));
	}

	if (!InParams.bProjectScales)
	{
		OutPoint.Transform.SetScale3D(InTransform.GetScale3D());
	}
	else
	{
		OutPoint.Transform.SetScale3D(Cache->TerrainTransform.GetScale3D());
	}

	// No layer-weight attributes are written: weights are sampled GPU-side by a separate node.
	return true;
}

void UPCGTerrainData::InitializeTargetMetadata(const FPCGInitializeFromDataParams& InParams, UPCGMetadata* MetadataToInitialize) const
{
	check(MetadataToInitialize);

	// This data registers no attributes of its own (layer weights removed — see ProjectPoint). The
	// override is kept so the base surface-data metadata initialization stays the single entry point
	// for any future per-cell attribute this data starts producing.
	Super::InitializeTargetMetadata(InParams, MetadataToInitialize);
}

UPCGSpatialData* UPCGTerrainData::CopyInternal(FPCGContext* Context) const
{
	UPCGTerrainData* NewData = FPCGContext::NewObject_AnyThread<UPCGTerrainData>(Context);

	CopyBaseSurfaceData(NewData);
	NewData->Cache = Cache;
	NewData->Props = Props;

	return NewData;
}

const UPCGPointData* UPCGTerrainData::CreatePointData(FPCGContext* Context) const
{
	return CastChecked<UPCGPointData>(CreateBasePointData(Context, UPCGPointData::StaticClass()), ECastCheckedType::NullAllowed);
}

const UPCGPointData* UPCGTerrainData::CreatePointData(FPCGContext* Context, const FBox& InBounds) const
{
	// SupportsBoundedPointData() stays false: the sampler always covers the full cache bounds.
	return CreatePointData(Context);
}

const UPCGPointArrayData* UPCGTerrainData::CreatePointArrayData(FPCGContext* Context, const FBox& InBounds) const
{
	return CastChecked<UPCGPointArrayData>(CreateBasePointData(Context, UPCGPointArrayData::StaticClass()), ECastCheckedType::NullAllowed);
}

const UPCGBasePointData* UPCGTerrainData::CreateBasePointData(FPCGContext* Context, TSubclassOf<UPCGBasePointData> PointDataClass) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGTerrainData::CreatePointData);

	// Default surface sampling over the full bounds (mirrors UPCGPolygon2DInteriorSurfaceData).
	const PCGSurfaceSampler::FSurfaceSamplerParams SamplerParams;
	return PCGSurfaceSampler::SampleSurface(Context, SamplerParams, this, /*InBoundingShape=*/nullptr, GetBounds(), PointDataClass);
}
