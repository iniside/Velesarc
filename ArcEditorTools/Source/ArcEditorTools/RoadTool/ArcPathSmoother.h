// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ArcRoadTool
{

enum class EArcSmoothingMethod : uint8
{
    Chaikin,
    CatmullRom
};

struct FArcSmoothParams
{
    EArcSmoothingMethod Method = EArcSmoothingMethod::Chaikin;
    int32 ChaikinIterations = 3;
    float ResampleSpacing = 200.0f;
};

TArray<FVector> SmoothPath(
    TConstArrayView<FVector> RawPath,
    const FArcSmoothParams& Params = FArcSmoothParams());

TArray<FVector> ResamplePathUniform(
    TConstArrayView<FVector> Path,
    float Spacing);

} // namespace ArcRoadTool
