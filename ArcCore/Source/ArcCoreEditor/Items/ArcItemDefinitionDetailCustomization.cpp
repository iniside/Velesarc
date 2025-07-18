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

#include "ArcItemDefinitionDetailCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "StructUtils/InstancedStruct.h"

TSharedRef<IDetailCustomization> FArcItemDefinitionDetailCustomization::MakeInstance()
{
	return MakeShared<FArcItemDefinitionDetailCustomization>();
}

void FArcItemDefinitionDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedPtr<IPropertyHandle> ItemFragmentSetHandle = DetailBuilder.GetProperty("FragmentSet");
	DetailBuilder.HideProperty(ItemFragmentSetHandle);
	
	TSharedPtr<IPropertyHandleSet> Set = ItemFragmentSetHandle->AsSet();
	uint32 SetNum = 0;
	Set->GetNumElements(SetNum);

	IDetailCategoryBuilder& Builder = DetailBuilder.EditCategory("Item Fragments");

	TSharedRef<SWidget> HeaderWidget = SNew(SOverlay)
		+SOverlay::Slot()
		[
			ItemFragmentSetHandle->CreateDefaultPropertyButtonWidgets()
		];
	Builder.HeaderContent(HeaderWidget);

	// So we can show edit buttons when there are no fragments added.
	if (SetNum == 0)
	{
		FDetailWidgetRow& EmptyRow = Builder.AddCustomRow(FText::FromString("Empty"));
		EmptyRow.ValueContent()
		[
			SNew(STextBlock).Text(FText::FromString("Add new fragment using + button above."))
		];
	}
	
	struct SetElement
	{
		FString Name;
		TSharedPtr<IPropertyHandle> Element;
	};
	
	TArray<SetElement> Uncategorized;
	// Might be less optimal, but we want to keep uncategorized together, directly in Parent category.
	for (uint32 Idx = 0; Idx < SetNum; Idx++)
	{
		TSharedPtr<IPropertyHandle> SetElem = Set->GetElement(Idx);

		TSharedPtr<IPropertyHandle> ChildHandle = SetElem->GetChildHandle(0);
		void* OutAddress = nullptr;
		ChildHandle->GetValueData(OutAddress);

		FInstancedStruct* IS = reinterpret_cast<FInstancedStruct*>(OutAddress);
		if (!IS || IS->GetScriptStruct() == nullptr)
		{
			Builder.AddProperty(SetElem);
			continue;
		}
		const FString& CategoryMetaData = IS->GetScriptStruct()->GetMetaData("Category");
		const FString& DisplayName = IS->GetScriptStruct()->GetMetaData("DisplayName");
		if (CategoryMetaData.Len() > 0)
		{
			continue;
		}
		SetElement NewElem;
		NewElem.Name = DisplayName.Len() > 0 ? DisplayName : IS->GetScriptStruct()->GetName();
		NewElem.Element = SetElem;
		
		Uncategorized.Add(NewElem);
	}

	Uncategorized.Sort([](const SetElement& Lhs, const SetElement& Rhs)
			{
				return Lhs.Name < Rhs.Name;
			});
	
	for (const SetElement& Elem : Uncategorized)
	{
		Builder.AddProperty(Elem.Element);
	}
	struct FragmentCategory
	{
		FString DisplayName;
		IDetailGroup* DetailGroup = nullptr;
		TArray<SetElement> ElementsToInsert;
		
		bool operator==(const FragmentCategory& Other) const
		{
			return DisplayName == Other.DisplayName;
		}

		bool operator==(const FString& Other) const
		{
			return DisplayName == Other;
		}
	};

	TArray<FragmentCategory> Categories;
	
	TMap<FString, IDetailGroup*> FragmentCategories;
	for (uint32 Idx = 0; Idx < SetNum; Idx++)
	{
		TSharedPtr<IPropertyHandle> SetElem = Set->GetElement(Idx);

		TSharedPtr<IPropertyHandle> ChildHandle = SetElem->GetChildHandle(0);
		
		void* OutAddress = nullptr;
		ChildHandle->GetValueData(OutAddress);

		FInstancedStruct* IS = reinterpret_cast<FInstancedStruct*>(OutAddress);
		if (!IS || IS->GetScriptStruct() == nullptr)
		{
			continue;
		}

		const FString& CategoryMetaData = IS->GetScriptStruct()->GetMetaData("Category");
		const FString& DisplayName = IS->GetScriptStruct()->GetMetaData("DisplayName");
		
		int32 PriorityValue = INDEX_NONE;
		// We consider priority as more important.
		if (CategoryMetaData.Len() > 0)
		{
			const int32 CategoryIdx = Categories.IndexOfByKey(CategoryMetaData);
			if (CategoryIdx != INDEX_NONE)
			{
				SetElement Elem;
				Elem.Name = DisplayName.Len() > 0 ? DisplayName : IS->GetScriptStruct()->GetName();
				Elem.Element = SetElem;
				
				Categories[CategoryIdx].ElementsToInsert.Add(Elem);
			}
			else
			{
				FragmentCategory NewCategory;

				NewCategory.DisplayName = CategoryMetaData;
				//NewCategory.CategoryName = NewCategory.DisplayName.Replace(TEXT(" "), TEXT(""));
				
				SetElement Elem;
				Elem.Name = DisplayName.Len() > 0 ? DisplayName : IS->GetScriptStruct()->GetName();
				Elem.Element = SetElem;
				NewCategory.ElementsToInsert.Add(Elem);

				Categories.Add(NewCategory);
			}
		}
	}

	Categories.Sort([](const FragmentCategory& Lhs, const FragmentCategory& Rhs)
	{
		// We want higher priorities first.
		return Lhs.DisplayName < Rhs.DisplayName;
	});

	for (FragmentCategory& Category : Categories)
	{
		// Delaying Details group insertion to have it in correct order.
		if (Category.DetailGroup == nullptr)
		{
			IDetailGroup& AddGroup = Builder.AddGroup(*Category.DisplayName, FText::FromString(Category.DisplayName), false, false);
			Category.DetailGroup = &AddGroup;
		}
		
		Category.ElementsToInsert.Sort([](const SetElement& Lhs, const SetElement& Rhs)
		{
			return Lhs.Name < Rhs.Name;
		});
		
		for (const SetElement& Element : Category.ElementsToInsert)
		{
			Category.DetailGroup->AddPropertyRow(Element.Element.ToSharedRef());	
		}	
		
	}
	
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailBuilder.GetSelectedObjects();
    UObject* ItemDefObject = nullptr;
    if (SelectedObjects.Num() > 0)
    {
    	// This details view only supports single object view. 
    	ItemDefObject = SelectedObjects[0].Get();
    }

	IDetailLayoutBuilder* Ptr = &DetailBuilder;
	
	Set->SetOnNumElementsChanged(FSimpleDelegate::CreateLambda([ItemFragmentSetHandle, ItemDefObject, Ptr]()
    {
    	FSetProperty* SetProp = CastField<FSetProperty>(ItemFragmentSetHandle->GetProperty());
    
		if (ItemDefObject)
		{
			void* SetData = nullptr;
			SetData = SetProp->ContainerPtrToValuePtr<void>(ItemDefObject);
		
			if (SetProp && SetData)
			{
				FScriptSetHelper Helper(SetProp, SetData);
				Helper.Rehash();
			}
		}
		
		Ptr->ForceRefreshDetails();
		//ItemFragmentSetHandle->RequestRebuildChildren();
    	
    }));

	ItemFragmentSetHandle->SetOnPropertyValueChangedWithData(
		TDelegate<void(const FPropertyChangedEvent&)>::CreateLambda([Ptr, ItemFragmentSetHandle, ItemDefObject](const FPropertyChangedEvent& InData)
			{
				// Just checked with debugger which property changes.
				if (InData.Property->GetFName() == TEXT("InstancedStruct"))
				{
					FSetProperty* SetProp = CastField<FSetProperty>(ItemFragmentSetHandle->GetProperty());

					if (ItemDefObject)
					{
						void* SetData = nullptr;
						SetData = SetProp->ContainerPtrToValuePtr<void>(ItemDefObject);
				
						if (SetProp && SetData)
						{
							FScriptSetHelper Helper(SetProp, SetData);
							Helper.Rehash();
						}
					}

					Ptr->ForceRefreshDetails();
					//ItemFragmentSetHandle->RequestRebuildChildren();
				}
			}));

	ItemFragmentSetHandle->SetOnChildPropertyValueChangedWithData( TDelegate<void(const FPropertyChangedEvent&)>::CreateLambda([Ptr, ItemFragmentSetHandle, ItemDefObject](const FPropertyChangedEvent& InData)
			{
				// Just checked with debugger which property changes.
				if (InData.Property->GetFName() == TEXT("InstancedStruct"))
				{
					FSetProperty* SetProp = CastField<FSetProperty>(ItemFragmentSetHandle->GetProperty());

					if (ItemDefObject)
					{
						void* SetData = nullptr;
						SetData = SetProp->ContainerPtrToValuePtr<void>(ItemDefObject);
				
						if (SetProp && SetData)
						{
							FScriptSetHelper Helper(SetProp, SetData);
							Helper.Rehash();
						}
					}

					//Ptr->ForceRefreshDetails();
					ItemFragmentSetHandle->RequestRebuildChildren();
				}
			})); 
}
