// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Preview/ArcRecoilPreviewSim.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UArcRecoilPreviewSettings;

enum class EArcRecoilPreviewPane : uint8
{
	Camera,
	Impact
};

class SArcRecoilPatternCanvas : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcRecoilPatternCanvas)
		: _Pane(EArcRecoilPreviewPane::Camera)
	{
	}
		SLATE_ARGUMENT(EArcRecoilPreviewPane, Pane)
		SLATE_ARGUMENT(TSharedPtr<const ArcRecoil::Preview::FArcRecoilPreviewResult>, Result)
		SLATE_ARGUMENT(TWeakObjectPtr<const UArcRecoilPreviewSettings>, Settings)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void RefreshFromResult(TSharedPtr<const ArcRecoil::Preview::FArcRecoilPreviewResult> InResult);

	void SetCurrentSample(float InCurrentSample);

protected:
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	virtual int32 OnPaint(const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	void UpdateScale();

	EArcRecoilPreviewPane Pane = EArcRecoilPreviewPane::Camera;
	TSharedPtr<const ArcRecoil::Preview::FArcRecoilPreviewResult> Result;
	TWeakObjectPtr<const UArcRecoilPreviewSettings> Settings;
	float CurrentSample = 0.0f;
	float HalfExtentDeg = 1.0f;
	mutable int32 LastWarnedSpreadSamples = INDEX_NONE;
};
