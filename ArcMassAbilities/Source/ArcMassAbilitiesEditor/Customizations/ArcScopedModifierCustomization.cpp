// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcScopedModifierCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcAttributeCapture.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FArcScopedModifierCustomization::MakeInstance()
{
	return MakeShareable(new FArcScopedModifierCustomization());
}

void FArcScopedModifierCustomization::RebuildCaptureOptions(TSharedRef<IPropertyHandle> StructPropertyHandle)
{
	CaptureOptions.Empty();

	TSharedPtr<IPropertyHandle> ParentHandle = StructPropertyHandle->GetParentHandle();
	if (!ParentHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IPropertyHandle> OuterHandle = ParentHandle->GetParentHandle();
	if (!OuterHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IPropertyHandle> CapturesHandle = OuterHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FArcEffectExecution, Captures));
	if (!CapturesHandle.IsValid())
	{
		return;
	}

	uint32 NumCaptures = 0;
	CapturesHandle->GetNumChildren(NumCaptures);

	for (uint32 Idx = 0; Idx < NumCaptures; ++Idx)
	{
		TSharedPtr<IPropertyHandle> ElementHandle = CapturesHandle->GetChildHandle(Idx);
		if (!ElementHandle.IsValid())
		{
			continue;
		}

		TSharedPtr<IPropertyHandle> AttrHandle = ElementHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcAttributeCaptureDef, Attribute));
		TSharedPtr<IPropertyHandle> SourceHandle = ElementHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcAttributeCaptureDef, CaptureSource));

		FString AttrDisplay = TEXT("Unknown");
		FString SourceDisplay = TEXT("?");

		if (AttrHandle.IsValid())
		{
			TSharedPtr<IPropertyHandle> FragTypeHandle = AttrHandle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcAttributeRef, FragmentType));
			TSharedPtr<IPropertyHandle> PropNameHandle = AttrHandle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcAttributeRef, PropertyName));

			UObject* FragTypeObj = nullptr;
			FName PropName = NAME_None;
			if (FragTypeHandle.IsValid())
			{
				FragTypeHandle->GetValue(FragTypeObj);
			}
			if (PropNameHandle.IsValid())
			{
				PropNameHandle->GetValue(PropName);
			}

			UScriptStruct* FragStruct = Cast<UScriptStruct>(FragTypeObj);
			if (FragStruct && PropName != NAME_None)
			{
				AttrDisplay = FString::Printf(TEXT("%s.%s"), *FragStruct->GetName(), *PropName.ToString());
			}
		}

		if (SourceHandle.IsValid())
		{
			uint8 SourceValue = 0;
			SourceHandle->GetValue(SourceValue);
			SourceDisplay = (SourceValue == static_cast<uint8>(EArcCaptureSource::Source)) ? TEXT("Source") : TEXT("Target");
		}

		FCaptureOption Option;
		Option.Index = static_cast<int32>(Idx);
		Option.DisplayName = FText::FromString(FString::Printf(TEXT("[%d] %s.%s"), Idx, *SourceDisplay, *AttrDisplay));
		CaptureOptions.Add(Option);
	}

	if (CaptureOptions.IsEmpty())
	{
		FCaptureOption Option;
		Option.Index = 0;
		Option.DisplayName = FText::FromString(TEXT("<No Captures>"));
		CaptureOptions.Add(Option);
	}
}

FText FArcScopedModifierCustomization::GetSelectedCaptureText() const
{
	if (!CaptureIndexHandle.IsValid())
	{
		return FText::FromString(TEXT("<Invalid>"));
	}

	int32 CurrentIndex = 0;
	CaptureIndexHandle->GetValue(CurrentIndex);

	for (const FCaptureOption& Option : CaptureOptions)
	{
		if (Option.Index == CurrentIndex)
		{
			return Option.DisplayName;
		}
	}

	return FText::FromString(FString::Printf(TEXT("<Invalid Index: %d>"), CurrentIndex));
}

void FArcScopedModifierCustomization::OnCaptureSelected(int32 NewIndex)
{
	if (CaptureIndexHandle.IsValid())
	{
		CaptureIndexHandle->SetValue(NewIndex);
	}
}

void FArcScopedModifierCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	CaptureIndexHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FArcScopedModifier, CaptureIndex));

	RebuildCaptureOptions(StructPropertyHandle);

	ComboItems.Empty();
	for (FCaptureOption& Opt : CaptureOptions)
	{
		ComboItems.Add(MakeShareable(new FCaptureOption(Opt)));
	}

	TSharedPtr<FCaptureOption> InitialSelection;
	if (CaptureIndexHandle.IsValid())
	{
		int32 CurrentIdx = 0;
		CaptureIndexHandle->GetValue(CurrentIdx);
		for (TSharedPtr<FCaptureOption>& Item : ComboItems)
		{
			if (Item->Index == CurrentIdx)
			{
				InitialSelection = Item;
				break;
			}
		}
	}
	if (!InitialSelection.IsValid() && ComboItems.Num() > 0)
	{
		InitialSelection = ComboItems[0];
	}

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.f)
	[
		SNew(SComboBox<TSharedPtr<FCaptureOption>>)
		.OptionsSource(&ComboItems)
		.InitiallySelectedItem(InitialSelection)
		.OnGenerateWidget_Lambda([](TSharedPtr<FCaptureOption> Item)
		{
			return SNew(STextBlock).Text(Item->DisplayName);
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<FCaptureOption> Item, ESelectInfo::Type)
		{
			if (Item.IsValid())
			{
				OnCaptureSelected(Item->Index);
			}
		})
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &FArcScopedModifierCustomization::GetSelectedCaptureText)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		]
	];
}

void FArcScopedModifierCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 Idx = 0; Idx < NumChildren; ++Idx)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(Idx);
		if (!ChildHandle.IsValid())
		{
			continue;
		}

		if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FArcScopedModifier, CaptureIndex))
		{
			continue;
		}

		StructBuilder.AddProperty(ChildHandle.ToSharedRef());
	}
}
