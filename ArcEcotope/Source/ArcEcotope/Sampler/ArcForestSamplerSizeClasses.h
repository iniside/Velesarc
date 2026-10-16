// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

struct FArcForestSamplerInput;

namespace UE::ArcForest::Sampler
{
	struct FArcForestSamplerBandTarget
	{
		int32 SpeciesIndex = INDEX_NONE;
		int32 BandIndex = INDEX_NONE;
		double MeanHeightCm = 0.0;
		double StemsPerCell = 0.0;
	};

	ARCECOTOPE_API void SolveBandTargets(
		const FArcForestSamplerInput& Input,
		TArrayView<const double> SpeciesWeights,
		double RecordCellAreaCm2,
		TArray<FArcForestSamplerBandTarget>& OutTargets,
		TArray<int32>& OutUnsolvedSpeciesIndices);
}
