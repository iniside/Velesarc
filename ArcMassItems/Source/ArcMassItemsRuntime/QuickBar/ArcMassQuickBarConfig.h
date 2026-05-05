// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MassEntityTypes.h"
#include "QuickBar/ArcMassQuickBarTypes.h"
#include "ArcMassQuickBarConfig.generated.h"

UCLASS(BlueprintType)
class ARCMASSITEMSRUNTIME_API UArcMassQuickBarConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TArray<FArcMassQuickBarEntry> QuickBars;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarTag : public FMassTag
{
	GENERATED_BODY()
};
