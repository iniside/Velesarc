// Copyright Lukasz Baran. All Rights Reserved.

#include "BoxShadow/UBoxShadow.h"
#include "BoxShadow/UBoxShadowSlot.h"
#include "Engine/Texture2D.h"
#include "Rendering/SlateRenderTransform.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBoxShadow)

#define LOCTEXT_NAMESPACE "ArcMarkupUI"

namespace UBoxShadowPrivate
{
	// The default soft drop-shadow brush registered in StarshipCoreStyle.
	static const FName DefaultShadowBrushName(TEXT("BoxShadow"));

	// ZOrder of the shadow layer; content sits above it (see UBoxShadowSlot).
	static constexpr int32 ShadowZOrder = 0;

	static FSlateBrush BuildTextureBrush(UTexture2D* InTexture, const FMargin& InMargin)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(InTexture);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = InMargin;
		return Brush;
	}
} // namespace UBoxShadowPrivate

UBoxShadow::UBoxShadow()
{
	bIsVariable = false;
	SetVisibilityInternal(ESlateVisibility::SelfHitTestInvisible);
}

bool UBoxShadow::RebuildShadowBrush()
{
	CachedShadowTexture = ShadowTexture;

	if (ShadowTexture != nullptr)
	{
		ShadowBrush = MakeShared<FSlateBrush>(UBoxShadowPrivate::BuildTextureBrush(ShadowTexture, ShadowTextureMargin));
		return true;
	}

	const FSlateBrush* DefaultBrush = FAppStyle::Get().GetBrush(UBoxShadowPrivate::DefaultShadowBrushName);

	// Null-check: stripped Slate content / unregistered brush — skip the shadow entirely.
	if (DefaultBrush == nullptr || DefaultBrush->GetResourceName() == NAME_None)
	{
		ShadowBrush.Reset();
		return false;
	}

	ShadowBrush = MakeShared<FSlateBrush>(*DefaultBrush);
	return true;
}

TSharedRef<SWidget> UBoxShadow::RebuildWidget()
{
	MyOverlay = SNew(SOverlay);
	MyShadowImage.Reset();
	MyShadowSlot = nullptr;

	RebuildShadowBrush();
	EnsureShadowLayer();

	if (GetChildrenCount() > 0)
	{
		CastChecked<UBoxShadowSlot>(GetContentSlot())->BuildSlot(MyOverlay.ToSharedRef());
	}

	UpdateShadowLayout();

	return MyOverlay.ToSharedRef();
}

void UBoxShadow::EnsureShadowLayer()
{
	if (MyShadowImage.IsValid() || !MyOverlay.IsValid() || !ShadowBrush.IsValid())
	{
		return;
	}

	MyShadowImage = SNew(SImage)
		.Image(ShadowBrush.Get())
		.ColorAndOpacity(FSlateColor(ShadowColor));

	// AddSlot(ZOrder) inserts sorted, so the shadow lands under the ZOrder-1 content
	// layer even when it is created after the content.
	MyOverlay->AddSlot(UBoxShadowPrivate::ShadowZOrder)
		.Expose(MyShadowSlot)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			MyShadowImage.ToSharedRef()
		];
}

void UBoxShadow::UpdateShadowLayout()
{
	if (MyShadowImage.IsValid())
	{
		MyShadowImage->SetColorAndOpacity(FSlateColor(ShadowColor));
		MyShadowImage->SetRenderTransform(FSlateRenderTransform(FVector2D(Offset)));
	}

	if (MyShadowSlot != nullptr)
	{
		// Negative padding grows the shadow beyond the content box on every side.
		MyShadowSlot->SetPadding(FMargin(-Spread));
	}
}

void UBoxShadow::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// Rebuild the brush if the texture override changed since the last build.
	if (CachedShadowTexture.Get() != ShadowTexture)
	{
		RebuildShadowBrush();
		if (MyShadowImage.IsValid())
		{
			MyShadowImage->SetImage(ShadowBrush.Get());
		}
	}

	EnsureShadowLayer();
	UpdateShadowLayout();
}

void UBoxShadow::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyOverlay.Reset();
	MyShadowImage.Reset();
	MyShadowSlot = nullptr;
	ShadowBrush.Reset();
}

// --- Getters ---

FVector2D UBoxShadow::GetOffset() const
{
	return Offset;
}

float UBoxShadow::GetSpread() const
{
	return Spread;
}

FLinearColor UBoxShadow::GetShadowColor() const
{
	return ShadowColor;
}

UTexture2D* UBoxShadow::GetShadowTexture() const
{
	return ShadowTexture;
}

FMargin UBoxShadow::GetShadowTextureMargin() const
{
	return ShadowTextureMargin;
}

// --- Setters ---

void UBoxShadow::SetOffset(FVector2D InOffset)
{
	Offset = InOffset;
	if (MyShadowImage.IsValid())
	{
		MyShadowImage->SetRenderTransform(FSlateRenderTransform(FVector2D(Offset)));
	}
}

void UBoxShadow::SetSpread(float InSpread)
{
	Spread = InSpread;
	if (MyShadowSlot != nullptr)
	{
		MyShadowSlot->SetPadding(FMargin(-Spread));
	}
}

void UBoxShadow::SetShadowColor(FLinearColor InShadowColor)
{
	ShadowColor = InShadowColor;
	if (MyShadowImage.IsValid())
	{
		MyShadowImage->SetColorAndOpacity(FSlateColor(ShadowColor));
	}
}

void UBoxShadow::SetShadowTexture(UTexture2D* InShadowTexture)
{
	ShadowTexture = InShadowTexture;
	RebuildShadowBrush();
	EnsureShadowLayer();
	if (MyShadowImage.IsValid())
	{
		MyShadowImage->SetImage(ShadowBrush.Get());
		UpdateShadowLayout();
	}
}

void UBoxShadow::SetShadowTextureMargin(FMargin InShadowTextureMargin)
{
	ShadowTextureMargin = InShadowTextureMargin;
	RebuildShadowBrush();
	if (MyShadowImage.IsValid())
	{
		MyShadowImage->SetImage(ShadowBrush.Get());
	}
}

UClass* UBoxShadow::GetSlotClass() const
{
	return UBoxShadowSlot::StaticClass();
}

void UBoxShadow::OnSlotAdded(UPanelSlot* InSlot)
{
	if (MyOverlay.IsValid())
	{
		CastChecked<UBoxShadowSlot>(InSlot)->BuildSlot(MyOverlay.ToSharedRef());
	}
}

void UBoxShadow::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Content sits above the shadow; clearing all non-shadow children removes it.
	if (MyOverlay.IsValid())
	{
		MyOverlay->RemoveSlot(1);
	}
}

#if WITH_EDITOR

const FText UBoxShadow::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

#undef LOCTEXT_NAMESPACE
