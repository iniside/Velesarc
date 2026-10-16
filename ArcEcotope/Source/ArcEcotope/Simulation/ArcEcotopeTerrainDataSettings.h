// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcEcotopeTerrainDataSettings.generated.h"

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcEcotopeTerrainDataSettings
{
	GENERATED_BODY()

	/** Soil depth a Deposits reading of 0 maps to; the SoilDepthMinM/MaxM clamps then apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "m"))
	float DepositsAtZeroDepthM = 0.0f;

	/** Soil depth a Deposits reading of 1 maps to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "m"))
	float DepositsAtMaxDepthM = 1.5f;

	/** Soil depth substituted at every cell when the Deposits map is absent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "m"))
	float MissingDepositsDepthM = 1.0f;

	/** Scales the RockMap into the field: 0 disables it, 1 makes a map reading of 1 fully bare. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RockWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water")
	bool bUseTreeRiverBuffer = true;

	/** Distance from the channel inside which trees are fully excluded; 0 leaves the channel cells themselves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.0", ClampMax = "10000.0", EditCondition = "bUseTreeRiverBuffer", Units = "cm"))
	float TreeBufferHardDistanceCm = 0.0f;

	/** Distance beyond the hard core over which the tree buffer fades linearly to nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.0", ClampMax = "10000.0", EditCondition = "bUseTreeRiverBuffer", Units = "cm"))
	float TreeBufferFalloffDistanceCm = 600.0f;

	/** Scales the Wear map into the field: 0 disables it, 1 makes a map reading of 1 a full suppression. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WearWeight = 1.0f;

	/** Scales the Scree map into the field: 0 disables it, 1 makes a map reading of 1 fully mobile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScreeWeight = 1.0f;

	/** Share of ground-cover yaw the flow heading takes: 0 is the random yaw as authored, 1 aligns every instance to the flow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlowAlignStrength = 0.5f;

	/** Checks every parameter against the range its clamp metadata advertises. */
	bool IsValid(FText& OutError) const;
};
