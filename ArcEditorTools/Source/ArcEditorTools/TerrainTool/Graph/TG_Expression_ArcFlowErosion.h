// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Expressions/TG_Expression.h"
#include "TG_Var.h"

#include "TG_Expression_ArcFlowErosion.generated.h"

UCLASS(MinimalAPI)
class UTG_Expression_ArcFlowErosion : public UTG_Expression
{
	GENERATED_BODY()

public:
	TG_DECLARE_EXPRESSION(FName(TEXT("Terrain")));

	// The input heightmap texture
	UPROPERTY(meta = (TGType = "TG_Input"))
	FTG_Texture Input;

	// The eroded heightmap result
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Output", PinDisplayName = "Eroded Height"))
	FTG_Texture ErodedHeight;

	// Flow-line intensity map
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Output", PinDisplayName = "Flow Map"))
	FTG_Texture FlowMap;

	// Number of erosion simulation steps
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "1", ClampMax = "1000"))
	int32 Iterations = 80;

	// Erosion strength (0 = none, 1 = maximum)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0", ClampMax = "1"))
	float Strength = 0.3f;

	// Sediment deposition rate (0 = none, 1 = maximum)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0", ClampMax = "1"))
	float Deposition = 0.5f;

	// Rainfall amount per step (0 = none, 1 = maximum)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0", ClampMax = "1"))
	float Rain = 0.5f;

	// Vertical scale applied to the input height inside the sim so flux/erosion are
	// non-negligible on a normalized 0..1 heightmap. The height output is un-scaled back.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "1", ClampMax = "8192"))
	float HeightScale = 256.0f;

	// Pre-log gain on accumulated flow throughput (log(1 + acc * FlowMapGain)); lower values suit the large dynamic range of the accumulator.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0", ClampMax = "1000"))
	float FlowMapGain = 1.0f;

	// Fraction of peak D8 drainage required for channel incision.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0", ClampMax = "1"))
	float ChannelThreshold = 0.03f;

	// World/basin scale for erosion features. Lower values keep fine tributaries;
	// higher values suppress small gullies and deepen fewer larger channels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0.25", ClampMax = "8"))
	float FeatureScale = 1.0f;

	// Fraction of the original height usable as the channel cut floor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0.1", ClampMax = "0.7"))
	float ChannelDepthFraction = 0.25f;

	// Shapes how quickly tributaries deepen relative to trunk channels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0.1", ClampMax = "2.0"))
	float DrainageExponent = 0.5f;

	// Maximum normalized height eroded by one pass before HeightScale is applied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0", ClampMax = "0.1"))
	float MaxErodePerStep = 0.01f;

	// Sediment carrying capacity multiplier for MFD drainage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0", ClampMax = "8"))
	float SedimentCapacity = 1.0f;

	// Small talus relaxation amount applied after channel carving.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0", ClampMax = "1"))
	float ThermalRate = 0.5f;

	virtual FTG_Name GetDefaultName() const override { return FName(TEXT("Flow Erosion")); }
	virtual FText GetTooltipText() const override;
	virtual void Evaluate(FTG_EvaluationContext* InContext) override;
};
