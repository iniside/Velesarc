// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemEconomyFragment.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Economy Pricing", Category = "Economy"))
struct ARCECONOMY_API FArcItemEconomyFragment : public FArcItemFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float BasePrice = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float MinPrice = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float MaxPrice = 100.0f;
};
