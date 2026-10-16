// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Fragments/ArcItemFragment.h"
#include "Items/ArcItemInstance.h"

#include "ArcRPGItemFragment_CascadingCost.generated.h"

USTRUCT()
struct FArcRPGItemInstance_CascadingCost : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	int32 CostStacks = 0;

public:
	int32 GetStacks() const { return CostStacks; }

	void AddStack() { CostStacks++; }

	void ResetStacks() { CostStacks = 0; }

	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		const FArcRPGItemInstance_CascadingCost& OtherCast = static_cast<const FArcRPGItemInstance_CascadingCost&>(Other);
		return CostStacks == OtherCast.CostStacks;
	}

	virtual bool ShouldPersist() const override { return false; }
};

USTRUCT(meta = (Category = "Cost"))
struct FArcRPGItemFragment_CascadingCost : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcRPGItemInstance_CascadingCost::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcRPGItemFragment_CascadingCost::StaticStruct();
	}

};
