// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

class SArcRmlPreviewBackground final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SArcRmlPreviewBackground)
	{
	}
	SLATE_END_ARGS()

	virtual ~SArcRmlPreviewBackground() override;

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	static TSharedRef<SWidget> MakeSettingsMenu();

protected:
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	void HandleSettingsChanged();

	FDelegateHandle SettingsChangedHandle;
	mutable FSlateBrush TextureBrush;
};
