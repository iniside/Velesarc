// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Targeting/ArcMassTargetData.h"

namespace ArcMassTargetDataInternal
{
    const TArray<FHitResult> EmptyHitResults;
}

// ----------------------------------------------------------------
// FArcMassTargetData
// ----------------------------------------------------------------

const TArray<FHitResult>& FArcMassTargetData::GetHitResults() const
{
    return ArcMassTargetDataInternal::EmptyHitResults;
}

// ----------------------------------------------------------------
// FArcMassTargetData_HitResults
// ----------------------------------------------------------------

FVector FArcMassTargetData_HitResults::GetEndPoint() const
{
    if (HitResults.Num() > 0)
    {
        return HitResults[0].ImpactPoint;
    }
    return FVector::ZeroVector;
}

// ----------------------------------------------------------------
// FArcMassTargetDataHandle
// ----------------------------------------------------------------

bool FArcMassTargetDataHandle::IsValid() const
{
    return Data.IsValid() && Data.GetPtr<FArcMassTargetData>() != nullptr;
}

bool FArcMassTargetDataHandle::HasHitResults() const
{
    const FArcMassTargetData* TargetData = Data.GetPtr<FArcMassTargetData>();
    return TargetData != nullptr && TargetData->HasHitResults();
}

const TArray<FHitResult>& FArcMassTargetDataHandle::GetHitResults() const
{
    const FArcMassTargetData* TargetData = Data.GetPtr<FArcMassTargetData>();
    if (TargetData != nullptr)
    {
        return TargetData->GetHitResults();
    }
    return ArcMassTargetDataInternal::EmptyHitResults;
}

FVector FArcMassTargetDataHandle::GetFirstImpactPoint() const
{
    const TArray<FHitResult>& HitResults = GetHitResults();
    if (HitResults.Num() > 0)
    {
        return HitResults[0].ImpactPoint;
    }
    return FVector::ZeroVector;
}

bool FArcMassTargetDataHandle::HasOrigin() const
{
    const FArcMassTargetData* TargetData = Data.GetPtr<FArcMassTargetData>();
    return TargetData != nullptr && TargetData->HasOrigin();
}

FVector FArcMassTargetDataHandle::GetOrigin() const
{
    const FArcMassTargetData* TargetData = Data.GetPtr<FArcMassTargetData>();
    if (TargetData != nullptr)
    {
        return TargetData->GetOrigin();
    }
    return FVector::ZeroVector;
}

bool FArcMassTargetDataHandle::HasEndPoint() const
{
    const FArcMassTargetData* TargetData = Data.GetPtr<FArcMassTargetData>();
    return TargetData != nullptr && TargetData->HasEndPoint();
}

FVector FArcMassTargetDataHandle::GetEndPoint() const
{
    const FArcMassTargetData* TargetData = Data.GetPtr<FArcMassTargetData>();
    if (TargetData != nullptr)
    {
        return TargetData->GetEndPoint();
    }
    return FVector::ZeroVector;
}

void FArcMassTargetDataHandle::SetHitResults(const TArray<FHitResult>& InHitResults)
{
    FArcMassTargetData_HitResults HitResultData;
    HitResultData.HitResults = InHitResults;
    Data.InitializeAs<FArcMassTargetData_HitResults>(MoveTemp(HitResultData));
}

void FArcMassTargetDataHandle::SetHitResults(TArray<FHitResult>&& InHitResults)
{
    FArcMassTargetData_HitResults HitResultData;
    HitResultData.HitResults = MoveTemp(InHitResults);
    Data.InitializeAs<FArcMassTargetData_HitResults>(MoveTemp(HitResultData));
}
