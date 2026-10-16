// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcScopedModifierCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcAttributeCapture.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Customizations/ArcEffectSummaryText.h"

TSharedRef<IPropertyTypeCustomization> FArcScopedModifierCustomization::MakeInstance()
{
	return MakeShareable(new FArcScopedModifierCustomization());
}

void FArcScopedModifierCustomization::RebuildCaptureOptions(TSharedRef<IPropertyHandle> StructPropertyHandle)
{
	CaptureOptions.Empty();

	const FArcEffectExecution* Execution = nullptr;
	TSharedPtr<IPropertyHandle> OuterHandle = StructPropertyHandle->GetParentHandle();
	while (OuterHandle.IsValid() && !Execution)
	{
		const FStructProperty* StructProp = CastField<FStructProperty>(OuterHandle->GetProperty());
		const bool bIsInstancedStruct = StructProp && StructProp->Struct == TBaseStructure<FInstancedStruct>::Get();
		const bool bIsExecution = StructProp && StructProp->Struct->IsChildOf(FArcEffectExecution::StaticStruct());
		if (bIsInstancedStruct || bIsExecution)
		{
			OuterHandle->EnumerateConstRawData(
				[&Execution, bIsInstancedStruct](const void* RawData, const int32 DataIndex, const int32 NumDatas)
				{
					if (!RawData)
					{
						return true;
					}
					if (bIsInstancedStruct)
					{
						Execution = static_cast<const FInstancedStruct*>(RawData)->GetPtr<FArcEffectExecution>();
					}
					else
					{
						Execution = static_cast<const FArcEffectExecution*>(RawData);
					}
					return false;
				});
		}
		OuterHandle = OuterHandle->GetParentHandle();
	}
	if (!Execution)
	{
		return;
	}

	TArray<FArcAttributeCaptureDef> GatheredCaptures;
	TArray<FArcExtraCaptureDef> GatheredExtras;
	Execution->GatherCaptures(GatheredCaptures, GatheredExtras);

	for (int32 Idx = 0; Idx < GatheredCaptures.Num(); ++Idx)
	{
		const FArcAttributeCaptureDef& Def = GatheredCaptures[Idx];
		const FString SourceDisplay = (Def.CaptureSource == EArcCaptureSource::Source) ? TEXT("Source") : TEXT("Target");
		FString AttrDisplay = TEXT("Unknown");
		if (Def.Attribute.FragmentType && Def.Attribute.PropertyName != NAME_None)
		{
			AttrDisplay = FString::Printf(TEXT("%s.%s"), *Def.Attribute.FragmentType->GetName(), *Def.Attribute.PropertyName.ToString());
		}

		FCaptureOption Option;
		Option.Index = Idx;
		Option.DisplayName = FText::FromString(FString::Printf(TEXT("[%d] %s.%s"), Idx, *SourceDisplay, *AttrDisplay));
		Option.Def = Def;
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
	if (!CaptureDef_FragTypeHandle.IsValid())
	{
		return FText::FromString(TEXT("<Invalid>"));
	}

	UObject* FragTypeObj = nullptr;
	CaptureDef_FragTypeHandle->GetValue(FragTypeObj);
	FName PropName = NAME_None;
	if (CaptureDef_PropNameHandle.IsValid())
	{
		CaptureDef_PropNameHandle->GetValue(PropName);
	}
	uint8 SourceValue = 0;
	if (CaptureDef_SourceHandle.IsValid())
	{
		CaptureDef_SourceHandle->GetValue(SourceValue);
	}
	bool SnapValue = true;
	if (CaptureDef_SnapshotHandle.IsValid())
	{
		CaptureDef_SnapshotHandle->GetValue(SnapValue);
	}

	FArcAttributeCaptureDef CurrentDef;
	CurrentDef.Attribute.FragmentType = Cast<UScriptStruct>(FragTypeObj);
	CurrentDef.Attribute.PropertyName = PropName;
	CurrentDef.CaptureSource = static_cast<EArcCaptureSource>(SourceValue);
	CurrentDef.bSnapshot = SnapValue;

	for (const FCaptureOption& Option : CaptureOptions)
	{
		if (Option.Def == CurrentDef)
		{
			return Option.DisplayName;
		}
	}

	return FText::FromString(TEXT("<Invalid Capture>"));
}

FText FArcScopedModifierCustomization::GetSummaryText() const
{
	const FText CaptureText = GetSelectedCaptureText();
	const FText OpText      = ArcEffectSummaryText::OperationToText(OperationHandle);
	const FText MagText     = ArcEffectSummaryText::BuildModifierMagnitude(
		TypeHandle, MagnitudeHandle, SetByCallerTagHandle, CustomMagnitudeHandle);

	FString Result;
	Result.Reserve(128);

	if (!CaptureText.IsEmpty())
	{
		Result += CaptureText.ToString();
	}
	if (!OpText.IsEmpty())
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT("  ");
		}
		Result += OpText.ToString();
	}
	if (!MagText.IsEmpty())
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT("  ");
		}
		Result += MagText.ToString();
	}

	return FText::FromString(Result);
}

void FArcScopedModifierCustomization::OnCaptureSelected(const FArcAttributeCaptureDef& NewDef)
{
	if (CaptureDef_FragTypeHandle.IsValid())
	{
		CaptureDef_FragTypeHandle->SetValue(static_cast<UObject*>(NewDef.Attribute.FragmentType));
	}
	if (CaptureDef_PropNameHandle.IsValid())
	{
		CaptureDef_PropNameHandle->SetValue(NewDef.Attribute.PropertyName);
	}
	if (CaptureDef_SourceHandle.IsValid())
	{
		CaptureDef_SourceHandle->SetValue(static_cast<uint8>(NewDef.CaptureSource));
	}
	if (CaptureDef_SnapshotHandle.IsValid())
	{
		CaptureDef_SnapshotHandle->SetValue(NewDef.bSnapshot);
	}
}

void FArcScopedModifierCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	CaptureDefHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FArcScopedModifier, CaptureDef));

	if (CaptureDefHandle.IsValid())
	{
		TSharedPtr<IPropertyHandle> AttrHandle = CaptureDefHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcAttributeCaptureDef, Attribute));
		if (AttrHandle.IsValid())
		{
			CaptureDef_FragTypeHandle = AttrHandle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcAttributeRef, FragmentType));
			CaptureDef_PropNameHandle = AttrHandle->GetChildHandle(
				GET_MEMBER_NAME_CHECKED(FArcAttributeRef, PropertyName));
		}
		CaptureDef_SourceHandle   = CaptureDefHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcAttributeCaptureDef, CaptureSource));
		CaptureDef_SnapshotHandle = CaptureDefHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcAttributeCaptureDef, bSnapshot));
	}

	OperationHandle       = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScopedModifier, Operation));
	TypeHandle            = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScopedModifier, Type));
	MagnitudeHandle       = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScopedModifier, Magnitude));
	SetByCallerTagHandle  = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScopedModifier, SetByCallerTag));
	CustomMagnitudeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScopedModifier, CustomMagnitude));

	RebuildCaptureOptions(StructPropertyHandle);

	ComboItems.Empty();
	for (FCaptureOption& Opt : CaptureOptions)
	{
		ComboItems.Add(MakeShareable(new FCaptureOption(Opt)));
	}

	TSharedPtr<FCaptureOption> InitialSelection;
	if (CaptureDef_FragTypeHandle.IsValid())
	{
		UObject* FragTypeObj = nullptr;
		CaptureDef_FragTypeHandle->GetValue(FragTypeObj);
		FName PropName = NAME_None;
		if (CaptureDef_PropNameHandle.IsValid())
		{
			CaptureDef_PropNameHandle->GetValue(PropName);
		}
		uint8 SourceValue = 0;
		if (CaptureDef_SourceHandle.IsValid())
		{
			CaptureDef_SourceHandle->GetValue(SourceValue);
		}
		bool SnapValue = true;
		if (CaptureDef_SnapshotHandle.IsValid())
		{
			CaptureDef_SnapshotHandle->GetValue(SnapValue);
		}

		FArcAttributeCaptureDef CurrentDef;
		CurrentDef.Attribute.FragmentType = Cast<UScriptStruct>(FragTypeObj);
		CurrentDef.Attribute.PropertyName = PropName;
		CurrentDef.CaptureSource = static_cast<EArcCaptureSource>(SourceValue);
		CurrentDef.bSnapshot = SnapValue;

		for (TSharedPtr<FCaptureOption>& Item : ComboItems)
		{
			if (Item->Def == CurrentDef)
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
		SNew(STextBlock)
		.Text(this, &FArcScopedModifierCustomization::GetSummaryText)
		.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
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
				OnCaptureSelected(Item->Def);
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
		if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FArcScopedModifier, CaptureDef))
		{
			continue;
		}

		StructBuilder.AddProperty(ChildHandle.ToSharedRef());
	}
}
