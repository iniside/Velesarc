// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Assets/ArcTerrainBakedField.h"
#include "ArcEcotopeTerrainWeightStamp.generated.h"

namespace ArcEcotopeTerrainWeight
{
	ARCECOTOPE_API FGuid ComputeFieldContentId(TConstArrayView<float> Values, const FIntPoint& Dims);
}

UCLASS(ClassGroup = (ArcEcotope), meta = (BlueprintSpawnableComponent, DisplayName = "Ecotope Terrain Weight Stamp"))
class ARCECOTOPE_API UArcEcotopeTerrainWeightStamp : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Channels", meta = (GetOptions = "GetTerrainChannels"))
	FName CanopyChannel;

	UPROPERTY(EditAnywhere, Category = "Channels", meta = (GetOptions = "GetTerrainChannels"))
	FName LitterChannel;

	/** Grows (positive) or shrinks (negative) the painted area before the falloff ramp is applied. */
	UPROPERTY(EditAnywhere, Category = "Mask", meta = (ForceUnits = "cm"))
	float MaskExpandCm = 0.0f;

	/** Width of the ramp from full override to untouched, measured outward from the expanded edge. Zero is a hard edge. */
	UPROPERTY(EditAnywhere, Category = "Mask", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaskFalloffCm = 0.0f;

	/** Ramp shape. 1 is linear; above 1 holds full coverage longer then falls off fast; below 1 does the opposite. */
	UPROPERTY(EditAnywhere, Category = "Mask", meta = (ClampMin = "0.1", ClampMax = "8.0"))
	float MaskFalloffExponent = 1.0f;

	UPROPERTY()
	TMap<FName, FArcTerrainBakedField> BakedLayers;

	UPROPERTY()
	TMap<FName, FName> LayerChannelMap;

	/** Channels this bake zeroes rather than paints; one shared all-zero raster serves them all. */
	UPROPERTY()
	TArray<FName> SuppressedChannels;

	UPROPERTY()
	FArcTerrainBakedField UnionMask;

	/** Rebuilds the provider mirrors; call after any write to BakedLayers, SuppressedChannels or UnionMask. */
	void RefreshProviderFields();

	/** Drops every baked layer, its channel mapping, the suppression set and the union mask, then re-composites. */
	void ClearBakedLayers();

	//~ Begin UArcTerrainStampComponent interface
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
	//~ End UArcTerrainStampComponent interface

	//~ Begin UObject interface
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;
#endif
	//~ End UObject interface

private:
	/** Refcounted mirror of BakedLayers: GetSourceDescriptor runs per dirty cell and the rasters are large. */
	TMap<FName, TSharedPtr<const FArcTerrainBakedField>> ProviderFields;

	TSharedPtr<const FArcTerrainBakedField> SuppressionProviderField;

	TSharedPtr<const FArcTerrainBakedField> MaskProviderField;
};
