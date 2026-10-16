// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcCraft/Recipe/ArcCraftContext.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "Items/ArcItemSpec.h"

#include "ArcCraftTestContextProbe.generated.h"

/**
 * Recipe modifier source that records the FArcCraftContext the output builder hands it and produces nothing.
 * Lets tests read the context UArcCraftTickProcessor builds at completion.
 */
USTRUCT(meta = (Hidden))
struct FArcRecipeModifierSource_TestContextProbe : public FArcRecipeModifierSource
{
	GENERATED_BODY()

	static TArray<int32>& CapturedSlotAmounts()
	{
		static TArray<int32> SlotAmounts;
		return SlotAmounts;
	}

	static int32& EvaluateCount()
	{
		static int32 Count = 0;
		return Count;
	}

	static void Reset()
	{
		CapturedSlotAmounts().Reset();
		EvaluateCount() = 0;
	}

	virtual TArray<FArcCraftPendingModifier> Evaluate(
		const TArray<FArcItemSpec>& ConsumedIngredients,
		const FArcCraftContext& Context) const override
	{
		CapturedSlotAmounts() = Context.SlotAmounts;
		++EvaluateCount();
		return TArray<FArcCraftPendingModifier>();
	}
};
