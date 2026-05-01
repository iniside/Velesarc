// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Custom Slate widget drawing a rolling-window line graph of reward history.
 * Two lines: settlement (blue) and faction (green).
 */
class SArcRewardChart : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SArcRewardChart) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void UpdateData(const TArray<float>& InSettlementRewards, const TArray<float>& InFactionRewards, int32 InTotalEpisodes);

    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
    static constexpr int32 VisibleWindow = 200;

    TArray<float> SettlementRewards;
    TArray<float> FactionRewards;
    int32 TotalEpisodes = 0;
};
