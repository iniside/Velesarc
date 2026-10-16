// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Data/PCGSurfaceData.h"
#include "Surface/ArcTerrainSurfaceSample.h"

#include "PCGTerrainData.generated.h"

class UPCGBasePointData;

/**
 * Sampling options for ArcTerrain PCG surface data; shared by the data and the Get-Terrain settings.
 *
 * DELIBERATELY EMPTY. Its only member was bGetLayerWeights, and CPU layer-weight sampling has been
 * removed outright (weights are sampled GPU-side by a separate node). The struct is KEPT
 * rather than deleted because it is a PCG_Overridable settings member of UPCGGetTerrainSettings;
 * removing it would churn the node's UI/override surface for no behavioural gain. Add future
 * sampling options here.
 */
USTRUCT(BlueprintType)
struct FPCGTerrainDataProps
{
	GENERATED_BODY()
};

/**
 * PCG surface-data view over an ArcTerrain actor. The per-cell read of the BAKED height tile
 * (ArcTerrainTileAccess::LoadCellHeightsSync) happens once at element execute into an immutable
 * FArcTerrainSurfaceCache; ProjectPoint then bilinear-samples that cache O(1) and is thread-safe
 * (read-only shared cache), mirroring the UPCGLandscapeData / FPCGLandscapeCacheEntry split. The
 * baked payload is cooked data, so editor, PIE and cooked all take the same single code path.
 *
 * Composite-raster caveat: the baked tile is on the terrain's COMPOSITE raster (CompositeQuads+3),
 * which may be coarser than the dense per-cell raster the emitted mesh was built from.
 *
 * MPA border caveat: cells built by MeshPartitionAdaptive render a coarse welded ring at cell
 * borders; this data always samples the full-res dense tile instead, so height/normal near MPA
 * cell borders may deviate from the rendered mesh by up to the tessellation error (bounded —
 * deliberate, see FArcTerrainSurfaceCellCache).
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class ARCPCGEXTENSION_API UPCGTerrainData : public UPCGSurfaceData
{
	GENERATED_BODY()

public:
	/** Takes the immutable cache + props; derives base surface Transform/LocalBounds from the cache. */
	void Initialize(TSharedPtr<const FArcTerrainSurfaceCache> InCache, FPCGTerrainDataProps InProps);

	//~Begin UPCGData interface
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	//~End UPCGData interface

	//~Begin UPCGSpatialData interface
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual FVector GetNormal() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual void InitializeTargetMetadata(const FPCGInitializeFromDataParams& InParams, UPCGMetadata* MetadataToInitialize) const override;

protected:
	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
	//~End UPCGSpatialData interface

public:
	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context, const FBox& InBounds) const override;
	virtual const UPCGPointArrayData* CreatePointArrayData(FPCGContext* Context, const FBox& InBounds) const override;
	//~End UPCGSpatialDataWithPointCache interface

protected:
	const UPCGBasePointData* CreateBasePointData(FPCGContext* Context, TSubclassOf<UPCGBasePointData> PointDataClass) const;

	/** Immutable, shared across copies; read-only after Initialize, so ProjectPoint is thread-safe. */
	TSharedPtr<const FArcTerrainSurfaceCache> Cache;

	UPROPERTY()
	FPCGTerrainDataProps Props;
};
