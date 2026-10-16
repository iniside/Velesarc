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

#include "Customizations/ArcItemDefinitionFragmentFocusCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyHandle.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment.h"

TSharedRef<IDetailCustomization> FArcItemDefinitionFragmentFocusCustomization::MakeInstance(TWeakPtr<FArcItemFragmentSelection> InSelection)
{
	return MakeShared<FArcItemDefinitionFragmentFocusCustomization>(InSelection);
}

FArcItemDefinitionFragmentFocusCustomization::FArcItemDefinitionFragmentFocusCustomization(TWeakPtr<FArcItemFragmentSelection> InSelection)
	: SelectionWeak(InSelection)
{
}

namespace ArcItemDefFragFocus
{
	static FName GetSetPropertyName(EArcFragmentSetType SetType)
	{
		switch (SetType)
		{
		case EArcFragmentSetType::Runtime:       return GET_MEMBER_NAME_CHECKED(FArcItemDefinitionData, FragmentSet);
		case EArcFragmentSetType::ScalableFloat: return GET_MEMBER_NAME_CHECKED(FArcItemDefinitionData, ScalableFloatFragmentSet);
		}
		return NAME_None;
	}
}

void FArcItemDefinitionFragmentFocusCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide every default category so only our "Fragment" category shows.
	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);
	for (const FName& Cat : CategoryNames)
	{
		DetailBuilder.HideCategory(Cat);
	}

	TSharedPtr<FArcItemFragmentSelection> Selection = SelectionWeak.Pin();
	if (!Selection.IsValid())
	{
		return;
	}

	const UScriptStruct* SelectedStruct = Selection->SelectedStruct.Get();
	if (SelectedStruct == nullptr)
	{
		return;
	}

	const FName SetPropName = ArcItemDefFragFocus::GetSetPropertyName(Selection->SetType);
	if (SetPropName.IsNone())
	{
		return;
	}

	TSharedRef<IPropertyHandle> SetHandle = DetailBuilder.GetProperty(SetPropName, FArcItemDefinitionData::StaticStruct());
	if (!SetHandle->IsValidHandle())
	{
		return;
	}

	// Iterate the TSet's children, find the FArcInstancedStruct whose inner script struct matches the selection.
	uint32 NumChildren = 0;
	SetHandle->GetNumChildren(NumChildren);
	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = SetHandle->GetChildHandle(Index);
		if (!ChildHandle.IsValid() || !ChildHandle->IsValidHandle())
		{
			continue;
		}

		void* RawData = nullptr;
		if (ChildHandle->GetValueData(RawData) != FPropertyAccess::Success || RawData == nullptr)
		{
			continue;
		}

		const FArcInstancedStruct* InstancedStruct = static_cast<const FArcInstancedStruct*>(RawData);
		const UScriptStruct* InnerStruct = InstancedStruct->GetScriptStruct();
		if (InnerStruct == SelectedStruct)
		{
			IDetailCategoryBuilder& Cat = DetailBuilder.EditCategory(TEXT("Fragment"));
			Cat.AddProperty(ChildHandle.ToSharedRef());
			break;
		}
	}
}
