// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Misc/Guid.h"
#include "UObject/SoftObjectPtr.h"

#include "ArcRiverGeneratorComponent.generated.h"

class AArcTerrain;
class AWaterZone;
class UArcRiverGeneratorComponent;

DECLARE_DELEGATE_RetVal_TwoParams(bool, FArcRiverGenerateDelegate,
	UArcRiverGeneratorComponent&, FText& /*OutError*/);

ARCRIVER_API extern FArcRiverGenerateDelegate GArcRiverCreateWaterZone;
ARCRIVER_API extern FArcRiverGenerateDelegate GArcRiverGenerate;
ARCRIVER_API extern FArcRiverGenerateDelegate GArcRiverClear;
ARCRIVER_API extern FArcRiverGenerateDelegate GArcRiverClearAll;

namespace ArcRiver
{
	/** Carried by every generated river on top of the per-generator tag, so tooling can tell a generated river from a
		hand-placed one without knowing which generator made it. */
	ARCRIVER_API extern const FName GeneratedTag;
}

UCLASS(ClassGroup = (ArcRiver), meta = (BlueprintSpawnableComponent, DisplayName = "ArcRiver Generator"))
class ARCRIVER_API UArcRiverGeneratorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Source")
	TSoftObjectPtr<AArcTerrain> AffectedTerrain;

	/** The zone every generated river registers with, through its water body's zone override. */
	UPROPERTY(EditAnywhere, Category = "Water")
	TSoftObjectPtr<AWaterZone> WaterZone;

	/** Mask value at which a texel counts as water. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float ChannelThreshold = 0.5f;

	/** 10-30 for alluvial rivers. */
	UPROPERTY(EditAnywhere, Category = "Channel", meta = (ClampMin = "4.0", ClampMax = "60.0"))
	float WidthToDepthRatio = 16.0f;

	UPROPERTY(EditAnywhere, Category = "Network", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinRiverLength = 20000.0f;

	/** Douglas-Peucker tolerance. */
	UPROPERTY(EditAnywhere, Category = "Network", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float SimplifyTolerance = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Network", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinHalfWidth = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Network", meta = (ClampMin = "1"))
	int32 MaxRivers = 256;

	/** Serialized rather than derived from the owner and component names: renaming either would change a name-derived
		tag and strand every river already placed under the old one. */
	UPROPERTY()
	FGuid GeneratorId;

	/** Places or re-fits WaterZone over the source modifier's footprint. */
	UFUNCTION(CallInEditor, Category = "ArcRiver")
	void CreateWaterZone();

	UFUNCTION(CallInEditor, Category = "ArcRiver")
	void GenerateRivers();

	UFUNCTION(CallInEditor, Category = "ArcRiver")
	void ClearRivers();

	/** Destroys every AWaterBodyRiver in the loaded level, including hand-placed ones and rivers no generator claims. */
	UFUNCTION(CallInEditor, Category = "ArcRiver")
	void ClearAllRivers();

	FGuid GetGeneratorId() const
	{
		return GeneratorId;
	}

	FGuid GetOrCreateGeneratorId();
};
