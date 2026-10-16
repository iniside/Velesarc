// Copyright Lukasz Baran. All Rights Reserved.

#include "Svg/ArcSvgImage.h"

#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Svg/ArcSvgAsset.h"
#include "Widgets/Layout/SScaleBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSvgImage)

DEFINE_LOG_CATEGORY_STATIC(LogArcSvgImage, Log, All);

void UArcSvgImage::SetSvgAsset(UArcSvgAsset* InSvgAsset)
{
	SvgAsset = InSvgAsset;
	RebuildSvgBrush();
}

UArcSvgAsset* UArcSvgImage::GetSvgAsset() const
{
	return SvgAsset;
}

void UArcSvgImage::SetIconSize(FVector2D InIconSize)
{
	IconSize = InIconSize;
	RebuildSvgBrush();
}

FVector2D UArcSvgImage::GetIconSize() const
{
	return IconSize;
}

void UArcSvgImage::SetOverrideDrawSize(bool bInOverrideDrawSize)
{
	bOverrideDrawSize = bInOverrideDrawSize;
	RebuildSvgBrush();
}

bool UArcSvgImage::GetOverrideDrawSize() const
{
	return bOverrideDrawSize;
}

void UArcSvgImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	RebuildSvgBrush();

	if (MyScaleBox.IsValid())
	{
		MyScaleBox->SetStretch(SvgAsset && SvgAsset->bPreserveAspectRatio ? EStretch::ScaleToFit : EStretch::Fill);
	}
}

TSharedRef<SWidget> UArcSvgImage::RebuildWidget()
{
	Super::RebuildWidget();

	MyScaleBox = SNew(SScaleBox)
		[
			MyImage.ToSharedRef()
		];

	return MyScaleBox.ToSharedRef();
}

void UArcSvgImage::ReleaseSlateResources(bool bReleaseChildren)
{
	MyScaleBox.Reset();

	Super::ReleaseSlateResources(bReleaseChildren);
}

void UArcSvgImage::RebuildSvgBrush()
{
	if (!SvgAsset)
	{
		// Clearing the asset must clear the brush too, or a recycled widget keeps
		// drawing the previous icon.
		SetBrush(FSlateNoResource());
		return;
	}

	const FString& Path = SvgAsset->GetOrExtractSvgFilePath();
	if (Path.IsEmpty())
	{
		UE_LOGF(LogArcSvgImage, Warning,"UArcSvgImage '%ls' could not resolve an SVG file path from asset '%ls'; leaving existing brush.",
			*GetPathName(), *SvgAsset->GetPathName());
		return;
	}

	const FVector2D EffectiveSize = bOverrideDrawSize ? IconSize : SvgAsset->DefaultDrawSize;

	FSlateVectorImageBrush VectorBrush(Path, EffectiveSize, SvgAsset->Tint);
	SetBrush(VectorBrush);
}
