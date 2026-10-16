// Copyright Epic Games, Inc. All Rights Reserved.

#include "TerrainTool/Graph/TG_Expression_ArcThermalErosion.h"
#include "T_ThermalErosion.h"
#include "2D/TextureHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TG_Expression_ArcThermalErosion)

FText UTG_Expression_ArcThermalErosion::GetTooltipText() const
{
	return FText::FromString(TEXT("Thermal (talus) erosion; relaxes slopes toward the talus angle."));
}

void UTG_Expression_ArcThermalErosion::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	if (Input)
	{
		ErodedHeight = T_ThermalErosion::Create(InContext->Cycle, Input, Iterations, Strength, InContext->TargetId);
	}
	else
	{
		ErodedHeight = TextureHelper::GetBlack();
	}
}
