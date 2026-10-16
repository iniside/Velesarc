// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/SArcRmlPreviewBackground.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Preview/ArcRmlPreviewSettings.h"
#include "PropertyCustomizationHelpers.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "ArcRmlPreviewBackground"

namespace ArcRmlPreviewBackgroundPrivate
{
	using FColorGetter = const FLinearColor& (UArcRmlPreviewSettings::*)() const;
	using FColorSetter = void (UArcRmlPreviewSettings::*)(const FLinearColor&);

	FReply OpenPicker(FColorGetter Getter, FColorSetter Setter)
	{
		FColorPickerArgs PickerArgs((UArcRmlPreviewSettings::Get().*Getter)(), FOnLinearColorValueChanged::CreateLambda([Setter](FLinearColor Color)
		{
			(UArcRmlPreviewSettings::Get().*Setter)(Color);
		}));
		PickerArgs.bOnlyRefreshOnMouseUp = true;
		PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateLambda([Setter](FLinearColor OriginalColor)
		{
			(UArcRmlPreviewSettings::Get().*Setter)(OriginalColor);
		});
		OpenColorPicker(PickerArgs);
		return FReply::Handled();
	}

	TSharedRef<SWidget> MakeColorBlock(FColorGetter Getter, FColorSetter Setter)
	{
		return SNew(SBox)
			.Padding(4.0f, 2.0f)
			[
				SNew(SColorBlock)
				.Size(FVector2D(96.0f, 18.0f))
				.ShowBackgroundForAlpha(false)
				.Color_Lambda([Getter]() -> FLinearColor
				{
					return (UArcRmlPreviewSettings::Get().*Getter)();
				})
				.OnMouseButtonDown_Lambda([Getter, Setter](const FGeometry&, const FPointerEvent&) -> FReply
				{
					return OpenPicker(Getter, Setter);
				})
			];
	}

	void AddModeEntry(FMenuBuilder& MenuBuilder, const FText& Label, EArcRmlPreviewBackgroundMode Mode)
	{
		MenuBuilder.AddMenuEntry(Label, FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Mode]()
				{
					UArcRmlPreviewSettings::Get().SetMode(Mode);
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Mode]() -> bool
				{
					return UArcRmlPreviewSettings::Get().GetMode() == Mode;
				})),
			NAME_None, EUserInterfaceActionType::RadioButton);
	}

	void AddDirectionEntry(FMenuBuilder& MenuBuilder, const FText& Label, EArcRmlPreviewGradientDirection Direction)
	{
		MenuBuilder.AddMenuEntry(Label, FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Direction]()
				{
					UArcRmlPreviewSettings::Get().SetGradientDirection(Direction);
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Direction]() -> bool
				{
					return UArcRmlPreviewSettings::Get().GetGradientDirection() == Direction;
				})),
			NAME_None, EUserInterfaceActionType::RadioButton);
	}
}

SArcRmlPreviewBackground::~SArcRmlPreviewBackground()
{
	if (UObjectInitialized())
	{
		UArcRmlPreviewSettings::Get().OnChanged().Remove(SettingsChangedHandle);
	}
}

void SArcRmlPreviewBackground::Construct(const FArguments& InArgs)
{
	SettingsChangedHandle = UArcRmlPreviewSettings::Get().OnChanged().AddSP(this, &SArcRmlPreviewBackground::HandleSettingsChanged);
}

int32 SArcRmlPreviewBackground::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	UArcRmlPreviewSettings& Settings = UArcRmlPreviewSettings::Get();
	switch (Settings.GetMode())
	{
	case EArcRmlPreviewBackgroundMode::SolidColor:
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), FAppStyle::GetBrush(TEXT("WhiteBrush")),
			ESlateDrawEffect::None, Settings.GetSolidColor());
		break;
	case EArcRmlPreviewBackgroundMode::Gradient:
	{
		const FVector2f LocalSize = AllottedGeometry.GetLocalSize();
		const bool bVertical = Settings.GetGradientDirection() == EArcRmlPreviewGradientDirection::Vertical;
		TArray<FSlateGradientStop> Stops;
		Stops.Emplace(FVector2f::ZeroVector, Settings.GetGradientStartColor());
		Stops.Emplace(bVertical ? FVector2f(0.0f, LocalSize.Y) : FVector2f(LocalSize.X, 0.0f), Settings.GetGradientEndColor());
		FSlateDrawElement::MakeGradient(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Stops),
			bVertical ? Orient_Horizontal : Orient_Vertical);
		break;
	}
	case EArcRmlPreviewBackgroundMode::Texture:
		if (UTexture2D* Texture = Settings.GetLoadedTexture())
		{
			TextureBrush.SetResourceObject(Texture);
			TextureBrush.SetImageSize(FVector2f(static_cast<float>(Texture->GetSizeX()), static_cast<float>(Texture->GetSizeY())));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &TextureBrush);
		}
		break;
	}
	return LayerId;
}

TSharedRef<SWidget> SArcRmlPreviewBackground::MakeSettingsMenu()
{
	FMenuBuilder MenuBuilder(false, nullptr);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("Mode", "Mode"));
	ArcRmlPreviewBackgroundPrivate::AddModeEntry(MenuBuilder, LOCTEXT("SolidColor", "Solid colour"), EArcRmlPreviewBackgroundMode::SolidColor);
	ArcRmlPreviewBackgroundPrivate::AddModeEntry(MenuBuilder, LOCTEXT("Gradient", "Gradient"), EArcRmlPreviewBackgroundMode::Gradient);
	ArcRmlPreviewBackgroundPrivate::AddModeEntry(MenuBuilder, LOCTEXT("Texture", "Texture"), EArcRmlPreviewBackgroundMode::Texture);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("SolidColorSection", "Solid colour"));
	MenuBuilder.AddWidget(ArcRmlPreviewBackgroundPrivate::MakeColorBlock(&UArcRmlPreviewSettings::GetSolidColor, &UArcRmlPreviewSettings::SetSolidColor),
		LOCTEXT("Color", "Colour"));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("GradientSection", "Gradient"));
	MenuBuilder.AddWidget(
		ArcRmlPreviewBackgroundPrivate::MakeColorBlock(&UArcRmlPreviewSettings::GetGradientStartColor, &UArcRmlPreviewSettings::SetGradientStartColor),
		LOCTEXT("StartColor", "Start"));
	MenuBuilder.AddWidget(
		ArcRmlPreviewBackgroundPrivate::MakeColorBlock(&UArcRmlPreviewSettings::GetGradientEndColor, &UArcRmlPreviewSettings::SetGradientEndColor),
		LOCTEXT("EndColor", "End"));
	ArcRmlPreviewBackgroundPrivate::AddDirectionEntry(MenuBuilder, LOCTEXT("Vertical", "Vertical (top to bottom)"), EArcRmlPreviewGradientDirection::Vertical);
	ArcRmlPreviewBackgroundPrivate::AddDirectionEntry(MenuBuilder, LOCTEXT("Horizontal", "Horizontal (left to right)"),
		EArcRmlPreviewGradientDirection::Horizontal);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("TextureSection", "Texture"));
	MenuBuilder.AddWidget(
		SNew(SBox)
		.MinDesiredWidth(280.0f)
		.Padding(4.0f, 2.0f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UTexture2D::StaticClass())
			.AllowClear(true)
			.DisplayThumbnail(true)
			.ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool())
			.ObjectPath_Lambda([]() -> FString
			{
				return UArcRmlPreviewSettings::Get().GetTexture().ToSoftObjectPath().ToString();
			})
			.OnObjectChanged_Lambda([](const FAssetData& AssetData)
			{
				UArcRmlPreviewSettings::Get().SetTexture(TSoftObjectPtr<UTexture2D>(AssetData.GetSoftObjectPath()));
			})
		],
		FText::GetEmpty(), true);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FVector2D SArcRmlPreviewBackground::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D::ZeroVector;
}

void SArcRmlPreviewBackground::HandleSettingsChanged()
{
	Invalidate(EInvalidateWidgetReason::Paint);
}

#undef LOCTEXT_NAMESPACE
