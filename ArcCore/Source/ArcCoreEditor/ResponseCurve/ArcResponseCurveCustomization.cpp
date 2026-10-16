// Copyright Lukasz Baran. All Rights Reserved.

#include "ResponseCurve/ArcResponseCurveCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "Math/ArcResponseCurve.h"
#include "PropertyHandle.h"
#include "ResponseCurve/SArcResponseCurvePreview.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcResponseCurveCustomization"

namespace ArcResponseCurveCustomizationPrivate
{
	struct FPreset
	{
		const TCHAR* Name;
		EArcResponseCurveType Type;
		float Slope;
		float Exponent;
		float XShift;
		float YShift;
	};

	const FPreset Presets[] =
	{
		{ TEXT("Basic linear"), EArcResponseCurveType::Linear, 1.0f, 0.0f, 0.0f, 0.0f },
		{ TEXT("Inverse linear"), EArcResponseCurveType::Linear, -1.0f, 0.0f, 0.0f, 1.0f },
		{ TEXT("Constant"), EArcResponseCurveType::Linear, 0.0f, 0.0f, 0.0f, 0.5f },
		{ TEXT("Standard cooldown"), EArcResponseCurveType::Polynomial, 1.0f, 6.0f, 0.0f, 0.0f },
		{ TEXT("Standard runtime"), EArcResponseCurveType::Polynomial, -1.0f, 6.0f, 0.0f, 1.0f },
		{ TEXT("Quadric lower left"), EArcResponseCurveType::Polynomial, 1.0f, 4.0f, 1.0f, 0.0f },
		{ TEXT("Quadric lower right"), EArcResponseCurveType::Polynomial, 1.0f, 4.0f, 0.0f, 0.0f },
		{ TEXT("Quadric upper left"), EArcResponseCurveType::Polynomial, -1.0f, 4.0f, 1.0f, 1.0f },
		{ TEXT("Quadric upper right"), EArcResponseCurveType::Polynomial, -1.0f, 4.0f, 0.0f, 1.0f },
		{ TEXT("Basic logistic"), EArcResponseCurveType::Logistic, 1.0f, 1.0f, 0.0f, 0.0f },
		{ TEXT("Inverse logistic"), EArcResponseCurveType::Logistic, -1.0f, 1.0f, 0.0f, 1.0f },
		{ TEXT("Basic logit"), EArcResponseCurveType::Logit, 1.0f, 1.0f, 0.0f, 0.0f },
		{ TEXT("Inverse logit"), EArcResponseCurveType::Logit, -1.0f, 1.0f, 0.0f, 0.0f },
		{ TEXT("Basic bell curve"), EArcResponseCurveType::Normal, 1.0f, 1.0f, 0.0f, 0.0f },
		{ TEXT("Inverse bell curve"), EArcResponseCurveType::Normal, -1.0f, 1.0f, 0.0f, 1.0f },
		{ TEXT("Basic sine wave"), EArcResponseCurveType::Sine, 1.0f, 1.0f, 0.0f, 0.0f },
		{ TEXT("Inverse sine wave"), EArcResponseCurveType::Sine, -1.0f, 1.0f, 0.0f, 0.0f },
	};

	constexpr int32 PresetCount = UE_ARRAY_COUNT(Presets);

	bool UsesExponent(const EArcResponseCurveType Type)
	{
		return Type == EArcResponseCurveType::Polynomial
			|| Type == EArcResponseCurveType::Logistic
			|| Type == EArcResponseCurveType::Normal;
	}

	bool MatchesPreset(const FPreset& Preset, const EArcResponseCurveType Type,
		const float Slope, const float Exponent, const float XShift, const float YShift)
	{
		return Preset.Type == Type
			&& FMath::IsNearlyEqual(Preset.Slope, Slope)
			&& (!UsesExponent(Type) || FMath::IsNearlyEqual(Preset.Exponent, Exponent))
			&& FMath::IsNearlyEqual(Preset.XShift, XShift)
			&& FMath::IsNearlyEqual(Preset.YShift, YShift);
	}

	FArcResponseCurve MakeCurve(const FPreset& Preset)
	{
		FArcResponseCurve Curve;
		Curve.Type = Preset.Type;
		Curve.Slope = Preset.Slope;
		Curve.Exponent = Preset.Exponent;
		Curve.XShift = Preset.XShift;
		Curve.YShift = Preset.YShift;
		return Curve;
	}

	const TArray<FArcResponseCurve>& GetPresetCurves()
	{
		static TArray<FArcResponseCurve> Curves;
		if (Curves.IsEmpty())
		{
			Curves.Reserve(PresetCount);
			for (const FPreset& Preset : Presets)
			{
				Curves.Add(MakeCurve(Preset));
			}
		}
		return Curves;
	}

	const FArcResponseCurve* GetPresetCurve(const int32 PresetIndex)
	{
		const TArray<FArcResponseCurve>& Curves = GetPresetCurves();
		return Curves.IsValidIndex(PresetIndex) ? &Curves[PresetIndex] : nullptr;
	}

	FText GetTypeDisplayName(const EArcResponseCurveType Type)
	{
		return StaticEnum<EArcResponseCurveType>()->GetDisplayNameTextByValue(static_cast<int64>(Type));
	}

	void SetPresetValues(const FPreset& Preset,
		const TSharedPtr<IPropertyHandle> TypeHandle,
		const TSharedPtr<IPropertyHandle> SlopeHandle,
		const TSharedPtr<IPropertyHandle> ExponentHandle,
		const TSharedPtr<IPropertyHandle> XShiftHandle,
		const TSharedPtr<IPropertyHandle> YShiftHandle)
	{
		FScopedTransaction Transaction(FText::Format(
			LOCTEXT("ApplyPresetTransaction", "Apply Response Curve Preset: {0}"), FText::FromString(Preset.Name)));

		SlopeHandle->SetValue(Preset.Slope);
		ExponentHandle->SetValue(Preset.Exponent);
		XShiftHandle->SetValue(Preset.XShift);
		YShiftHandle->SetValue(Preset.YShift);
		TypeHandle->SetValue(static_cast<uint8>(Preset.Type));
	}
}

TSharedRef<IPropertyTypeCustomization> FArcResponseCurveCustomization::MakeInstance()
{
	return MakeShared<FArcResponseCurveCustomization>();
}

void FArcResponseCurveCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructHandle = StructPropertyHandle;
	TypeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcResponseCurve, Type));
	SlopeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcResponseCurve, Slope));
	ExponentHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcResponseCurve, Exponent));
	XShiftHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcResponseCurve, XShift));
	YShiftHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcResponseCurve, YShift));
	CurveInfoHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcResponseCurve, CurveInfo));

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.IsEnabled(this, &FArcResponseCurveCustomization::IsEditable)
				.OnGetMenuContent(this, &FArcResponseCurveCustomization::BuildPresetMenu)
				.ToolTipText(LOCTEXT("PresetTooltip", "Fill in Type, Slope (m), Exponent (k), X Shift (c) and Y Shift (b) from a preset. The values stay editable afterwards."))
				.ButtonContent()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(this, &FArcResponseCurveCustomization::GetPresetButtonText)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SArcResponseCurvePreview)
				.OnGetCurve(this, &FArcResponseCurveCustomization::GetFirstCurve)
				.DesiredSize(FVector2D(48.0f, 24.0f))
				.bShowLabels(false)
				.ToolTipText(LOCTEXT("ThumbnailTooltip", "Response curve over input 0..1."))
			]
		];
}

void FArcResponseCurveCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex);
		if (ChildHandle.IsValid())
		{
			StructBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}

	StructBuilder.AddCustomRow(LOCTEXT("PreviewSearch", "Preview"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("PreviewLabel", "Preview"))
		]
		.ValueContent()
		.MinDesiredWidth(240.0f)
		.MaxDesiredWidth(420.0f)
		[
			SNew(SBox)
			.HeightOverride(140.0f)
			.Padding(0.0f, 2.0f)
			[
				SNew(SArcResponseCurvePreview)
				.OnGetCurve(this, &FArcResponseCurveCustomization::GetFirstCurve)
				.ToolTipText(LOCTEXT("PreviewTooltip", "x is the input (0..1), y is the score. With several objects selected, shows the first one."))
			]
		];
}

const FArcResponseCurve* FArcResponseCurveCustomization::GetFirstCurve() const
{
	if (!StructHandle.IsValid() || !StructHandle->IsValidHandle())
	{
		return nullptr;
	}

	TArray<const void*> RawData;
	StructHandle->AccessRawData(RawData);
	for (const void* Data : RawData)
	{
		if (Data != nullptr)
		{
			return static_cast<const FArcResponseCurve*>(Data);
		}
	}
	return nullptr;
}

TSharedRef<SWidget> FArcResponseCurveCustomization::BuildPresetMenu()
{
	using namespace ArcResponseCurveCustomizationPrivate;

	FMenuBuilder MenuBuilder(true, nullptr);

	int32 PresetIndex = 0;
	while (PresetIndex < PresetCount)
	{
		const EArcResponseCurveType GroupType = Presets[PresetIndex].Type;
		MenuBuilder.BeginSection(NAME_None, GetTypeDisplayName(GroupType));

		for (; PresetIndex < PresetCount && Presets[PresetIndex].Type == GroupType; ++PresetIndex)
		{
			const FPreset& Preset = Presets[PresetIndex];
			const FText Tooltip = FText::Format(
				LOCTEXT("PresetEntryTooltip", "{0}: m = {1}, k = {2}, c = {3}, b = {4}"),
				GetTypeDisplayName(Preset.Type),
				FText::AsNumber(Preset.Slope),
				FText::AsNumber(Preset.Exponent),
				FText::AsNumber(Preset.XShift),
				FText::AsNumber(Preset.YShift));

			MenuBuilder.AddMenuEntry(
				FUIAction(FExecuteAction::CreateSP(this, &FArcResponseCurveCustomization::ApplyPreset, PresetIndex)),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 1.0f, 6.0f, 1.0f)
				[
					SNew(SArcResponseCurvePreview)
					.OnGetCurve(FArcOnGetResponseCurve::CreateStatic(&GetPresetCurve, PresetIndex))
					.DesiredSize(FVector2D(40.0f, 20.0f))
					.bShowLabels(false)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Preset.Name))
				],
				NAME_None,
				Tooltip);
		}

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void FArcResponseCurveCustomization::ApplyPreset(int32 PresetIndex)
{
	using namespace ArcResponseCurveCustomizationPrivate;

	if (PresetIndex < 0 || PresetIndex >= PresetCount)
	{
		return;
	}

	const TSharedPtr<IPropertyHandle> Handles[] = { TypeHandle, SlopeHandle, ExponentHandle, XShiftHandle, YShiftHandle };
	for (const TSharedPtr<IPropertyHandle>& Handle : Handles)
	{
		if (!Handle.IsValid() || !Handle->IsValidHandle())
		{
			return;
		}
	}

	SetPresetValues(Presets[PresetIndex], TypeHandle, SlopeHandle, ExponentHandle, XShiftHandle, YShiftHandle);
}

FText FArcResponseCurveCustomization::GetPresetButtonText() const
{
	using namespace ArcResponseCurveCustomizationPrivate;

	if (!TypeHandle.IsValid() || !TypeHandle->IsValidHandle())
	{
		return FText::GetEmpty();
	}

	uint8 TypeValue = 0;
	if (TypeHandle->GetValue(TypeValue) == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	const EArcResponseCurveType Type = static_cast<EArcResponseCurveType>(TypeValue);
	if (Type == EArcResponseCurveType::Custom)
	{
		return LOCTEXT("CustomCurve", "Custom curve");
	}

	float Slope = 0.0f;
	float Exponent = 0.0f;
	float XShift = 0.0f;
	float YShift = 0.0f;
	const bool bAnyMultiple = SlopeHandle->GetValue(Slope) == FPropertyAccess::MultipleValues
		|| ExponentHandle->GetValue(Exponent) == FPropertyAccess::MultipleValues
		|| XShiftHandle->GetValue(XShift) == FPropertyAccess::MultipleValues
		|| YShiftHandle->GetValue(YShift) == FPropertyAccess::MultipleValues;
	if (bAnyMultiple)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	for (const FPreset& Preset : Presets)
	{
		if (MatchesPreset(Preset, Type, Slope, Exponent, XShift, YShift))
		{
			return FText::FromString(Preset.Name);
		}
	}

	return FText::Format(LOCTEXT("TunedType", "{0} (tuned)"), GetTypeDisplayName(Type));
}

bool FArcResponseCurveCustomization::IsEditable() const
{
	return StructHandle.IsValid() && StructHandle->IsValidHandle() && StructHandle->IsEditable();
}

#undef LOCTEXT_NAMESPACE
