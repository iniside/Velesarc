/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"
#include "ArcItemFragment.h"
#include "Items/ArcItemInstance.h"
#include "ArcItemFragment_Stacks.generated.h"

USTRUCT()
struct ARCCORE_API FArcItemInstance_Stacks : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

	friend struct FArcItemFragment_Stacks;
	
protected:
	UPROPERTY(EditAnywhere)
	int32 Stacks = 1;

public:
	//virtual void OnItemInitialize(const FArcItemData* InItem) override;
	
	int32 GetStacks() const
	{
		return Stacks;
	}

	void AddStacks(int32 NumStacks)
	{
		Stacks += NumStacks;
	}

	void SetStacks(int32 NumStacks)
	{
		Stacks = NumStacks;
	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemInstance_Stacks::StaticStruct();
	}
	
	virtual TSharedPtr<FArcItemInstance> Duplicate() const override
	{
		void* Allocated = FMemory::Malloc(GetScriptStruct()->GetCppStructOps()->GetSize(), GetScriptStruct()->GetCppStructOps()->GetAlignment());
		GetScriptStruct()->GetCppStructOps()->Construct(Allocated);
		TSharedPtr<FArcItemInstance_Stacks> SharedPtr = MakeShareable(static_cast<FArcItemInstance_Stacks*>(Allocated));

		SharedPtr->Stacks = Stacks;

		return SharedPtr;
	}

	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		const FArcItemInstance_Stacks& OtherInstance = static_cast<const FArcItemInstance_Stacks&>(Other);
		return Stacks == OtherInstance.Stacks; 
	}

	virtual bool ShouldPersist() const override { return true; }
};
/**
 * 
 */
USTRUCT()
struct ARCCORE_API FArcItemFragment_Stacks : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	int32 InitialStacks = 1;
	
public:
	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_Stacks::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemInstance_Stacks::StaticStruct();
	}
	
	virtual void OnItemAdded(const FArcItemData* InItem) const override;
};
