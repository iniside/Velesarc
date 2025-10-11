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

#include "ArcItemData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcItemsBPF.generated.h"

struct FArcItemId;
struct FArcItemData;
struct FArcItemDataHandle;
class UScriptStruct;
class UArcItemDefinition;

/**
 *
 */
UCLASS()
class ARCCORE_API UArcItemsBPF : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure
		, Category = "Arc Core|Items")
	static const bool EqualId(const FArcItemId& A
							  , const FArcItemId& B);
							  
	UFUNCTION(BlueprintCallable, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core", meta = (CustomStructureParam = "OutFragment", ExpandBoolAsExecs = "ReturnValue"))
    static bool GetItemFragment(const FArcItemDataHandle& Item
                              							, UPARAM(meta = (MetaStruct = "/Script/ArcCore.ArcItemFragment")) UScriptStruct* InFragmentType
                              							, int32& OutFragment);
	DECLARE_FUNCTION(execGetItemFragment);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Items")
	static FArcItemId GetItemId(const FArcItemDataHandle& Item);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core|Items")
	static FGameplayTag GetItemSlotId(const FArcItemDataHandle& Item);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Items")
	static const UArcItemDefinition* GetItemDefinition(const FArcItemDataHandle& Item);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	static float FindItemScalableValue(const FArcItemDataHandle& Item,
		UPARAM(meta = (MetaStruct = "/Script/ArcCore.ArcScalableFloatItemFragment")) UScriptStruct* InFragmentType, FName PropName);
	
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Items", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	static bool ItemHasTag(const FArcItemDataHandle& InItem, FGameplayTag InTag, bool bExact);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Items", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	static bool ItemHasAnyTag(const FArcItemDataHandle& InItem, FGameplayTagContainer InTag, bool bExact);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Items", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	static bool ItemHasAllTags(const FArcItemDataHandle& InItem, FGameplayTagContainer InTag, bool bExact);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	static int32 GetItemStacks(const FArcItemDataHandle& Item);
};
