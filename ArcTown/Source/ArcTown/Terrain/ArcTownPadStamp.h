// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stamps/ArcTerrainStampComponent.h"

#include "ArcTownPadStamp.generated.h"

/** Pads sit in their own band above the town's path stamps: where a path meets a building, the floor wins. */
inline constexpr double ArcTownPadStampPriority = 105.0;

/** Pad components are named from this and the building index, so their composite order is stable across sessions. */
inline constexpr TCHAR ArcTownPadComponentPrefix[] = TEXT("ArcTownPad_");

inline constexpr TCHAR ArcTownRoadComponentName[] = TEXT("ArcTownRoads");

UCLASS(ClassGroup = (ArcTown), meta = (DisplayName = "ArcTown Pad Stamp"))
class ARCTOWN_API UArcTownPadStamp : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	UArcTownPadStamp();

	/** From ArcTownStampKeys::MakePadContentId: it keys the terrain cache, which never hashes the raster. */
	UPROPERTY()
	FGuid PadContentId;

	/** Inward ramp from the footprint edge; the pad reaches full strength on CoverageXY shrunk by it on each side. */
	UPROPERTY()
	float EdgeFalloffBand = 0.0f;

	//~ Begin UArcTerrainStampComponent interface
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
	//~ End UArcTerrainStampComponent interface
};
