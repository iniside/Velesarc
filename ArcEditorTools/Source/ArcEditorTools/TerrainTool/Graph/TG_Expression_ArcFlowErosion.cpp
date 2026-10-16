// Copyright Epic Games, Inc. All Rights Reserved.

#include "TerrainTool/Graph/TG_Expression_ArcFlowErosion.h"
#include "T_FlowErosion.h"
#include "2D/TextureHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TG_Expression_ArcFlowErosion)

FText UTG_Expression_ArcFlowErosion::GetTooltipText() const
{
	return FText::FromString(TEXT("GPU MFD/D8 terrain erosion; outputs eroded height and accumulated flow map."));
}

void UTG_Expression_ArcFlowErosion::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	if (Input)
	{
		FArcFlowErosionResult Result = T_FlowErosion::Create(
			InContext->Cycle,
			Input,
			Iterations,
			Strength,
			Rain,
			Deposition,
			HeightScale,
			FlowMapGain,
			ChannelThreshold,
			FeatureScale,
			ChannelDepthFraction,
			DrainageExponent,
			MaxErodePerStep,
			SedimentCapacity,
			ThermalRate,
			InContext->TargetId);
		ErodedHeight = Result.ErodedHeight;
		FlowMap = Result.FlowMap;
	}
	else
	{
		ErodedHeight = TextureHelper::GetBlack();
		FlowMap = TextureHelper::GetBlack();
	}
}
