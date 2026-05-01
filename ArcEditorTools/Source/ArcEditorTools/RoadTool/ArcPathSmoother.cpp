// Copyright Lukasz Baran. All Rights Reserved.

#include "RoadTool/ArcPathSmoother.h"

namespace ArcRoadTool
{

namespace SmootherInternal
{

TArray<FVector> ApplyChaikin(TConstArrayView<FVector> Points, int32 Iterations)
{
    if (Points.Num() < 3)
    {
        return TArray<FVector>(Points.GetData(), Points.Num());
    }

    TArray<FVector> Current(Points.GetData(), Points.Num());

    for (int32 Iter = 0; Iter < Iterations; ++Iter)
    {
        TArray<FVector> Next;
        Next.Reserve(Current.Num() * 2);

        Next.Add(Current[0]);

        for (int32 Idx = 0; Idx < Current.Num() - 1; ++Idx)
        {
            FVector P0 = Current[Idx];
            FVector P1 = Current[Idx + 1];
            Next.Add(FMath::Lerp(P0, P1, 0.25f));
            Next.Add(FMath::Lerp(P0, P1, 0.75f));
        }

        Next.Add(Current.Last());

        Current = MoveTemp(Next);
    }

    return Current;
}

FVector EvalCatmullRom(
    const FVector& P0, const FVector& P1,
    const FVector& P2, const FVector& P3,
    float T)
{
    float T2 = T * T;
    float T3 = T2 * T;
    return 0.5f * (
        (2.0f * P1) +
        (-P0 + P2) * T +
        (2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * T2 +
        (-P0 + 3.0f * P1 - 3.0f * P2 + P3) * T3);
}

TArray<FVector> ApplyCatmullRom(TConstArrayView<FVector> Points, float Spacing)
{
    if (Points.Num() < 2)
    {
        return TArray<FVector>(Points.GetData(), Points.Num());
    }

    float TotalLength = 0.0f;
    for (int32 Idx = 0; Idx < Points.Num() - 1; ++Idx)
    {
        TotalLength += FVector::Dist(Points[Idx], Points[Idx + 1]);
    }

    if (Spacing <= 0.0f || TotalLength <= 0.0f)
    {
        return TArray<FVector>(Points.GetData(), Points.Num());
    }

    TArray<FVector> Result;
    Result.Reserve(FMath::CeilToInt32(TotalLength / Spacing) + 2);
    Result.Add(Points[0]);

    float AccumulatedDist = 0.0f;
    float NextEmitDist = Spacing;

    for (int32 SegIdx = 0; SegIdx < Points.Num() - 1; ++SegIdx)
    {
        int32 I0 = FMath::Max(SegIdx - 1, 0);
        int32 I1 = SegIdx;
        int32 I2 = SegIdx + 1;
        int32 I3 = FMath::Min(SegIdx + 2, Points.Num() - 1);

        float SegLength = FVector::Dist(Points[I1], Points[I2]);
        float SegStart = AccumulatedDist;
        float SegEnd = AccumulatedDist + SegLength;

        while (NextEmitDist < SegEnd)
        {
            float LocalT = (NextEmitDist - SegStart) / FMath::Max(SegLength, KINDA_SMALL_NUMBER);
            LocalT = FMath::Clamp(LocalT, 0.0f, 1.0f);

            FVector Point = EvalCatmullRom(
                Points[I0], Points[I1], Points[I2], Points[I3], LocalT);
            Result.Add(Point);
            NextEmitDist += Spacing;
        }

        AccumulatedDist = SegEnd;
    }

    if (FVector::Dist(Result.Last(), Points.Last()) > KINDA_SMALL_NUMBER)
    {
        Result.Add(Points.Last());
    }

    return Result;
}

} // namespace SmootherInternal

TArray<FVector> ResamplePathUniform(TConstArrayView<FVector> Path, float Spacing)
{
    if (Path.Num() < 2 || Spacing <= 0.0f)
    {
        return TArray<FVector>(Path.GetData(), Path.Num());
    }

    TArray<FVector> Result;

    float TotalLength = 0.0f;
    for (int32 Idx = 0; Idx < Path.Num() - 1; ++Idx)
    {
        TotalLength += FVector::Dist(Path[Idx], Path[Idx + 1]);
    }

    Result.Reserve(FMath::CeilToInt32(TotalLength / Spacing) + 2);
    Result.Add(Path[0]);

    float AccumulatedDist = 0.0f;
    float NextEmitDist = Spacing;
    int32 SegIdx = 0;
    float SegStart = 0.0f;

    while (SegIdx < Path.Num() - 1)
    {
        float SegLength = FVector::Dist(Path[SegIdx], Path[SegIdx + 1]);
        float SegEnd = SegStart + SegLength;

        while (NextEmitDist <= SegEnd && SegLength > KINDA_SMALL_NUMBER)
        {
            float Alpha = (NextEmitDist - SegStart) / SegLength;
            Result.Add(FMath::Lerp(Path[SegIdx], Path[SegIdx + 1], Alpha));
            NextEmitDist += Spacing;
        }

        SegStart = SegEnd;
        ++SegIdx;
    }

    if (FVector::Dist(Result.Last(), Path.Last()) > KINDA_SMALL_NUMBER)
    {
        Result.Add(Path.Last());
    }

    return Result;
}

TArray<FVector> SmoothPath(TConstArrayView<FVector> RawPath, const FArcSmoothParams& Params)
{
    if (RawPath.Num() < 2)
    {
        return TArray<FVector>(RawPath.GetData(), RawPath.Num());
    }

    switch (Params.Method)
    {
    case EArcSmoothingMethod::Chaikin:
    {
        TArray<FVector> Smoothed = SmootherInternal::ApplyChaikin(RawPath, Params.ChaikinIterations);
        if (Params.ResampleSpacing > 0.0f)
        {
            return ResamplePathUniform(Smoothed, Params.ResampleSpacing);
        }
        return Smoothed;
    }
    case EArcSmoothingMethod::CatmullRom:
        return SmootherInternal::ApplyCatmullRom(RawPath, Params.ResampleSpacing);
    }

    return TArray<FVector>(RawPath.GetData(), RawPath.Num());
}

} // namespace ArcRoadTool
