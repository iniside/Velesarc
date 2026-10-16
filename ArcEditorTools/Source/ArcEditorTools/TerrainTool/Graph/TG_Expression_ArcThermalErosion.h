// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Expressions/TG_Expression.h"
#include "TG_Var.h"

#include "TG_Expression_ArcThermalErosion.generated.h"

UCLASS(MinimalAPI)
class UTG_Expression_ArcThermalErosion : public UTG_Expression
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

	// Number of erosion passes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "1", ClampMax = "1000"))
	int32 Iterations = 50;

	// Talus slope relaxation strength (0 = none, 1 = maximum)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = "0", ClampMax = "1"))
	float Strength = 0.5f;

	virtual FTG_Name GetDefaultName() const override { return FName(TEXT("Thermal Erosion")); }
	virtual FText GetTooltipText() const override;
	virtual void Evaluate(FTG_EvaluationContext* InContext) override;
};
