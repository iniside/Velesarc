// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassTargetData.generated.h"

/**
 * Base struct for polymorphic targeting results.
 * Concrete subclasses add specific data (hit results, locations, entity arrays).
 * Stored inside FInstancedStruct for StateTree bindability.
 */
USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcMassTargetData
{
    GENERATED_BODY()

    virtual ~FArcMassTargetData() = default;

    virtual bool HasHitResults() const { return false; }
    virtual const TArray<FHitResult>& GetHitResults() const;

    virtual bool HasOrigin() const { return false; }
    virtual FVector GetOrigin() const { return FVector::ZeroVector; }

    virtual bool HasEndPoint() const { return false; }
    virtual FVector GetEndPoint() const { return FVector::ZeroVector; }
};

/**
 * Targeting result containing line trace / sweep hit results.
 */
USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcMassTargetData_HitResults : public FArcMassTargetData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FHitResult> HitResults;

    virtual bool HasHitResults() const override { return HitResults.Num() > 0; }
    virtual const TArray<FHitResult>& GetHitResults() const override { return HitResults; }

    virtual bool HasEndPoint() const override { return HitResults.Num() > 0; }
    virtual FVector GetEndPoint() const override;
};

/**
 * Handle wrapping an FInstancedStruct of FArcMassTargetData.
 * Provides a concrete type for StateTree property binding and delegates
 * accessors to the polymorphic target data inside.
 */
USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcMassTargetDataHandle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FInstancedStruct Data;

    bool IsValid() const;

    bool HasHitResults() const;
    const TArray<FHitResult>& GetHitResults() const;
    FVector GetFirstImpactPoint() const;

    bool HasOrigin() const;
    FVector GetOrigin() const;

    bool HasEndPoint() const;
    FVector GetEndPoint() const;

    template<typename T>
    const T* GetAs() const { return Data.GetPtr<T>(); }

    void SetHitResults(const TArray<FHitResult>& InHitResults);
    void SetHitResults(TArray<FHitResult>&& InHitResults);
};
