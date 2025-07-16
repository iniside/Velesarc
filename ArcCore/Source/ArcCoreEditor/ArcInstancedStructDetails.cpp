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

#include "ArcInstancedStructDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/UserDefinedStruct.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "IPropertyUtilities.h"
#include "ISinglePropertyView.h"
#include "IStructureDataProvider.h"
#include "IStructureDetailsView.h"
#include "StructUtils/InstancedStruct.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "ArcCoreEditor" 

//void FArcInstancedStructDataDetails::OnChildPropertyFound(IDetailChildrenBuilder& ChildrenBuilder
//	, TSharedPtr<IPropertyHandle> ChildHandle)
//{
//	FName CategoryRow = ChildHandle->GetDefaultCategoryName();
//	FText CategoryName = ChildHandle->GetDefaultCategoryText();
//	TSharedPtr<IPropertyHandle> ParentHandle = ChildHandle->GetParentHandle();
//	TSharedPtr<IPropertyHandleStruct> ParentStructHandle = ParentHandle->AsStruct();
//	bool bIsCategory = true;
//	if (ParentStructHandle.IsValid())
//	{
//		FName StructName = ParentStructHandle->GetStructData()->GetStruct()->GetFName();
//		if (CategoryRow == StructName)
//		{
//			bIsCategory = false;
//		}
//	}
//	if (bIsCategory == false)
//	{
//		ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef());
//	}
//	else
//	{
//		IDetailGroup*& DetailGroup = AddedGroups.FindOrAdd(CategoryRow);
//		if (DetailGroup == nullptr)
//		{
//			DetailGroup = &ChildrenBuilder.AddGroup(CategoryRow, CategoryName);
//		}
//		
//		DetailGroup->AddPropertyRow(ChildHandle.ToSharedRef());
//	}
//}

FArcInstancedStructDetails::FArcInstancedStructDetails()
{
	if(InstancedStructDetails.IsValid() == false)
	{
		InstancedStructDetails = MakeShareable(new FInstancedStructDetails);
	}
}

TSharedRef<IPropertyTypeCustomization> FArcInstancedStructDetails::MakeInstance()
{
	return MakeShared<FArcInstancedStructDetails>();
}

void FArcInstancedStructDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle
												 , class FDetailWidgetRow& HeaderRow
												 , IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	ArcStructHandle = StructPropertyHandle;
	InstanceStructHandle = StructPropertyHandle->GetChildHandle("InstancedStruct");

	FProperty* MetaDataProperty = StructPropertyHandle->GetMetaDataProperty();

	FProperty* CustomizedMetaDataProperty = InstanceStructHandle->GetMetaDataProperty();
	
	const FString& BaseStructName = MetaDataProperty->GetMetaData(TEXT("BaseStruct"));
	CustomizedMetaDataProperty->SetMetaData(TEXT("BaseStruct"), *BaseStructName);

	const bool bShowTreeView = MetaDataProperty->HasMetaData("ShowTreeView");
	if (bShowTreeView)
	{
		CustomizedMetaDataProperty->SetMetaData(TEXT("ShowTreeView"),"");
	}
	
	const bool bExcludeBaseStruct = MetaDataProperty->HasMetaData("ExcludeBaseStruct");
	if (bExcludeBaseStruct)
	{
		CustomizedMetaDataProperty->SetMetaData(TEXT("ExcludeBaseStruct"),"");
	}

	const FString& CategoryName = MetaDataProperty->GetMetaData("Category");
	if (CategoryName.Len() > 0)
	{
		CustomizedMetaDataProperty->SetMetaData(TEXT("Category"),*CategoryName);
	}
	bool bIsInSet = false;
	
	TSharedPtr<IPropertyHandle> ParentHandle = StructPropertyHandle->GetParentHandle();
	if(ParentHandle.IsValid())
	{
		TSharedPtr<IPropertyHandleSet> ParentSetHandle = ParentHandle->AsSet();
		if(ParentSetHandle.IsValid())
		{
			bIsInSet = true;
		}
	}
	
	
	if (bIsInSet == false)
	{
		// If we are in Array, just use normal customization.
		InstancedStructDetails->CustomizeHeader(InstanceStructHandle.ToSharedRef(), HeaderRow, StructCustomizationUtils);
	}
	else
	{
		bool bIsInlineDetails = false;
		
		TSharedPtr<class IStructureDetailsView>  DetailsView;
		TArray<void*> RawData;
		
		InstanceStructHandle->AccessRawData(RawData);
		for (int32 Idx = 0; Idx < RawData.Num(); ++Idx)
		{
			FInstancedStruct* IS = static_cast<FInstancedStruct*>(RawData[Idx]);
			if (IS == nullptr)
			{
				continue;
			}

			if (!IS->GetScriptStruct() || !IS->GetMutableMemory())
			{
				continue;
			}
			
			bIsInlineDetails = IS->GetScriptStruct()->HasMetaData("InlineDetails");

			TSharedPtr<FStructOnScope> Result = MakeShared<FStructOnScope>(IS->GetScriptStruct(), IS->GetMutableMemory());

			TSharedPtr<class IStructureDataProvider> InStruct = MakeShared<FStructOnScopeStructureDataProvider>(Result);
			
			FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
			
			FDetailsViewArgs DArgs;
			DArgs.bShowOptions = false;
			DArgs.bAllowSearch = false;
			DArgs.bAllowFavoriteSystem = false;
			DArgs.bShowSectionSelector = false;
			DArgs.bShowLooseProperties = true;
			DArgs.bShowObjectLabel = false;
			DArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::HideNameArea;
			
			FStructureDetailsViewArgs StructArgs;
			DetailsView = PropertyEditor.CreateStructureProviderDetailView(DArgs, StructArgs, InStruct);

			break;
		}
		
		/**
		 * This looks like some sheangas, but TSet<> does not have indexes,
		 * so we just put Values from row into WholeRow of customized property.
		 */
		FDetailWidgetRow InstancedHeaderRow;
		InstancedStructDetails->CustomizeHeader(InstanceStructHandle.ToSharedRef(), InstancedHeaderRow, StructCustomizationUtils);
		TSharedPtr<IPropertyHandleStruct> SH = InstanceStructHandle->AsStruct();

		if (bIsInlineDetails)
		{
			TSharedPtr<SHorizontalBox> HBox = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					[
						InstancedHeaderRow.ValueWidget.Widget
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						StructPropertyHandle->CreateDefaultPropertyButtonWidgets()
					]	
				];
			
			HBox->AddSlot()
			[
				DetailsView->GetWidget().ToSharedRef()
			];
			
			HeaderRow.WholeRowContent()
			[
				HBox.ToSharedRef()
			];
		}
		else
		{
			HeaderRow.WholeRowContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					InstancedHeaderRow.ValueWidget.Widget
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					StructPropertyHandle->CreateDefaultPropertyButtonWidgets()
				]
			];
		}
	}
}

void FArcInstancedStructDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle
												   , class IDetailChildrenBuilder& StructBuilder
												   , IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	bool bIsInlineDetails = false;
	
	TArray<void*> RawData;
	InstanceStructHandle->AccessRawData(RawData);
	for (int32 Idx = 0; Idx < RawData.Num(); ++Idx)
	{
		FInstancedStruct* IS = static_cast<FInstancedStruct*>(RawData[Idx]);
		if (IS == nullptr)
		{
			continue;
		}

		if (!IS->GetScriptStruct() || !IS->GetMutableMemory())
		{
			continue;
		}
		
		bIsInlineDetails = IS->GetScriptStruct()->HasMetaData("InlineDetails");
		break;
	}
	
	//if (!bIsInlineDetails)
	{
		TSharedRef<FArcInstancedStructDataDetails> DataDetails = MakeShared<FArcInstancedStructDataDetails>(InstanceStructHandle);
		StructBuilder.AddCustomBuilder(DataDetails);
	}
}

TSharedRef<IDetailCustomization> FArcItemFragment_MakeLocationInfoDetailCustomization::MakeInstance()
{
	return MakeShareable(new FArcItemFragment_MakeLocationInfoDetailCustomization());
}

void FArcItemFragment_MakeLocationInfoDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedPtr<IPropertyHandle> TransformerHandle = DetailBuilder.GetProperty("Transformer");
	DetailBuilder.HideProperty(TransformerHandle);

	IDetailCategoryBuilder& NoneCategory = DetailBuilder.EditCategoryAllowNone(NAME_None);

	IDetailPropertyRow& Row = NoneCategory.AddProperty(TransformerHandle);
	TSharedPtr<SWidget> NameWidget, ValueWidget;
	Row.GetDefaultWidgets(NameWidget, ValueWidget, false);
	
	NoneCategory.HeaderContent(ValueWidget.ToSharedRef(), true);
	NoneCategory.SetIsEmpty(true);
}

#undef LOCTEXT_NAMESPACE
