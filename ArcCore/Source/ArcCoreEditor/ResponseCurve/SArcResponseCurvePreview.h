// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

struct FArcResponseCurve;

DECLARE_DELEGATE_RetVal(const FArcResponseCurve*, FArcOnGetResponseCurve);

class ARCCOREEDITOR_API SArcResponseCurvePreview : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SArcResponseCurvePreview)
		: _DesiredSize(FVector2D(240.0f, 140.0f))
		, _bShowLabels(true)
	{
	}
		SLATE_EVENT(FArcOnGetResponseCurve, OnGetCurve)
		SLATE_ARGUMENT(FVector2D, DesiredSize)
		SLATE_ARGUMENT(bool, bShowLabels)
		SLATE_ATTRIBUTE(TOptional<float>, Marker)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

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
	FArcOnGetResponseCurve OnGetCurve;
	FVector2D DesiredSize = FVector2D(240.0f, 140.0f);
	bool bShowLabels = true;
	TAttribute<TOptional<float>> Marker;
	mutable TArray<float> SampleValues;
	mutable TArray<FVector2f> Line;
};
