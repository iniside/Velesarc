/**
 * This file is part of Velesarc
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
#include "ArcCore/Items/Fragments/ArcItemFragment.h"
#include "Templates/SubclassOf.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcMapAbilityEffectSpecContainer.h"
#include "ArcItemFragment_AbilityEffectsToApply.generated.h"

class UGameplayEffect;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcMapEffectItem
{
	GENERATED_BODY()

public:
	// All of these tags, must be present on source. (Owner of this item).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	FGameplayTagContainer SourceRequiredTags;

	// None of these tags must be present;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	FGameplayTagContainer SourceIgnoreTags;

	// All of those tags must be present.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	FGameplayTagContainer TargetRequiredTags;

	// None of these tags must be present;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	FGameplayTagContainer TargetIgnoreTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	TArray<TSubclassOf<UGameplayEffect>> Effects;

	FArcMapEffectItem()
		: Effects()
	{
	}
};

USTRUCT()
struct ARCCORE_API FArcItemInstance_EffectToApply : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

	friend struct FArcItemFragment_AbilityEffectsToApply;
	
protected:
	FArcMapAbilityEffectSpecContainer DefaultEffectSpecs;

public:
	//virtual void OnItemInitialize(const FArcItemData* InItem) override;
	//virtual void OnItemChanged(const FArcItemData* InItem) override;

	TArray<const FArcEffectSpecItem*> GetEffectSpecHandles(const FGameplayTag& InTag) const
	{
		TArray<const FArcEffectSpecItem*> Out;
		for (int32 Idx = 0; Idx < DefaultEffectSpecs.SpecsArray.Num(); Idx++)
		{
			if (DefaultEffectSpecs.SpecsArray[Idx].SpecTag.MatchesTag(InTag))
			{
				Out.Add(&DefaultEffectSpecs.SpecsArray[Idx]);
			}
		}
		return Out;;
	}

	const TArray<FArcEffectSpecItem>& GetAllEffectSpecHandles() const
	{
		return DefaultEffectSpecs.SpecsArray;
	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemInstance_EffectToApply::StaticStruct();
	}
	
	// This should prevent replication at all, since it is always true;
	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		return true;
	}
	
	//virtual TSharedPtr<FArcItemInstance> Duplicate() const override
	//{
	//	void* Allocated = FMemory::Malloc(GetScriptStruct()->GetCppStructOps()->GetSize(), GetScriptStruct()->GetCppStructOps()->GetAlignment());
	//	GetScriptStruct()->GetCppStructOps()->Construct(Allocated);
	//	TSharedPtr<FArcItemInstance_EffectToApply> SharedPtr = MakeShareable(static_cast<FArcItemInstance_EffectToApply*>(Allocated));
	//
	//	SharedPtr->DefaultEffectSpecs = DefaultEffectSpecs;
	//
	//	return SharedPtr;
	//}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Ability Effects To Apply"))
struct ARCCORE_API FArcItemFragment_AbilityEffectsToApply : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "GameplayEffect", ForceInlineRow), Category = "Config")
	TMap<FGameplayTag, FArcMapEffectItem> Effects;

	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_EffectToApply::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_AbilityEffectsToApply::StaticStruct();
	}
	
	virtual ~FArcItemFragment_AbilityEffectsToApply() override = default;
	
	virtual void OnItemInitialize(const FArcItemData* InItem) const override;
	virtual void OnItemChanged(const FArcItemData* InItem) const override;
};
