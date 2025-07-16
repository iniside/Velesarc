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

#include "ArcItemsBPF.h"

#include "ArcItemScalableFloat.h"
#include "ArcItemsHelpers.h"
#include "Fragments/ArcItemFragment_Tags.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemData.h"

const bool UArcItemsBPF::EqualId(const FArcItemId& A
								 , const FArcItemId& B)
{
	return A == B;
}

bool UArcItemsBPF::GetItemFragment(const FArcItemDataHandle& Item
								   , UPARAM(meta = (BaseStruct = "ArcItemFragment")) UScriptStruct* InFragmentType
								   , int32& OutFragment)
{
	checkNoEntry();
	return false;
}

DEFINE_FUNCTION(UArcItemsBPF::execGetItemFragment)
{
	P_GET_STRUCT(FArcItemDataHandle, Item);
	P_GET_OBJECT(UScriptStruct, InFragmentType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;
	bool bSuccess = true;

	{
		P_NATIVE_BEGIN;
		const uint8* Fragment = Item->GetItemDefinition()->GetFragment(InFragmentType);
		UScriptStruct* OutputStruct = OutItemProp->Struct;
		// Make sure the type we are trying to get through the blueprint node matches the
		// type of the message payload received.
		if ((OutItemProp != nullptr) && (OutItemProp->Struct != nullptr) && (Fragment != nullptr) && (
				OutItemProp->Struct == InFragmentType))
		{
			OutItemProp->Struct->CopyScriptStruct(OutItemDataPtr
				, Fragment);
			bSuccess = true;
		}
		P_NATIVE_END;
	}
	*(bool*)RESULT_PARAM = bSuccess;
}

FArcItemId UArcItemsBPF::GetItemId(const FArcItemDataHandle& Item)
{
	return Item->GetItemId();
}

FGameplayTag UArcItemsBPF::GetItemSlotId(const FArcItemDataHandle& Item)
{
	return Item->GetSlotId();
}

const UArcItemDefinition* UArcItemsBPF::GetItemDefinition(const FArcItemDataHandle& Item)
{
	return Item->GetItemDefinition();
}

float UArcItemsBPF::FindItemScalableValue(const FArcItemDataHandle& Item
	, UScriptStruct* InFragmentType
	, FName PropName)
{
	FStructProperty* Prop = FindFProperty<FStructProperty>(InFragmentType, PropName);
	FArcScalableCurveFloat Data(Prop, InFragmentType);
	
	return Item->GetItemDefinition()->GetValue(Data);
}

bool UArcItemsBPF::ItemHasTag(const FArcItemDataHandle& InItem
							  , FGameplayTag InTag, bool bExact)
{
	if (const FArcItemFragment_Tags* Tags = ArcItems::FindFragment<FArcItemFragment_Tags>(InItem.Get()))
	{
		if (bExact)
		{
			return Tags->ItemTags.HasTagExact(InTag);
		}
		else
		{
			return Tags->ItemTags.HasTag(InTag);	
		}
	}

	return false;
}

bool UArcItemsBPF::ItemHasAnyTag(const FArcItemDataHandle& InItem
	, FGameplayTagContainer InTag, bool bExact)
{
	if (const FArcItemFragment_Tags* Tags = ArcItems::FindFragment<FArcItemFragment_Tags>(InItem.Get()))
	{
		if (bExact)
		{
			return Tags->ItemTags.HasAnyExact(InTag);
		}
		else
		{
			return Tags->ItemTags.HasAny(InTag);	
		}
	}

	return false;
}

bool UArcItemsBPF::ItemHasAallTags(const FArcItemDataHandle& InItem
	, FGameplayTagContainer InTag, bool bExact)
{
	if (const FArcItemFragment_Tags* Tags = ArcItems::FindFragment<FArcItemFragment_Tags>(*InItem))
	{
		if (bExact)
		{
			return Tags->ItemTags.HasAllExact(InTag);
		}
		else
		{
			return Tags->ItemTags.HasAll(InTag);	
		}
	}

	return false;
}
