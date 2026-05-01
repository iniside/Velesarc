// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyEditor/Training/SArcRewardChart.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

void SArcRewardChart::Construct(const FArguments& InArgs)
{
}

void SArcRewardChart::UpdateData(
    const TArray<float>& InSettlementRewards,
    const TArray<float>& InFactionRewards,
    int32 InTotalEpisodes)
{
    SettlementRewards = InSettlementRewards;
    FactionRewards = InFactionRewards;
    TotalEpisodes = InTotalEpisodes;
}

FVector2D SArcRewardChart::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    return FVector2D(400.0f, 200.0f);
}

int32 SArcRewardChart::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const
{
    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
    const float Margin = 30.0f;
    const float ChartLeft = Margin;
    const float ChartRight = LocalSize.X - 10.0f;
    const float ChartTop = 10.0f;
    const float ChartBottom = LocalSize.Y - Margin;
    const float ChartWidth = ChartRight - ChartLeft;
    const float ChartHeight = ChartBottom - ChartTop;

    if (ChartWidth <= 0.0f || ChartHeight <= 0.0f)
    {
        return LayerId;
    }

    // Draw background
    FSlateDrawElement::MakeBox(
        OutDrawElements, LayerId,
        AllottedGeometry.ToPaintGeometry(),
        FCoreStyle::Get().GetBrush("GenericWhiteBox"),
        ESlateDrawEffect::None,
        FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));

    // Determine visible range
    const int32 NumPoints = FMath::Min(SettlementRewards.Num(), VisibleWindow);
    if (NumPoints < 2)
    {
        return LayerId;
    }

    const int32 StartIdx = FMath::Max(0, SettlementRewards.Num() - VisibleWindow);

    // Find Y range across both datasets
    float MinY = MAX_FLT;
    float MaxY = -MAX_FLT;
    for (int32 i = StartIdx; i < SettlementRewards.Num(); ++i)
    {
        const float FactionVal = FactionRewards.IsValidIndex(i) ? FactionRewards[i] : 0.0f;
        MinY = FMath::Min(MinY, FMath::Min(SettlementRewards[i], FactionVal));
        MaxY = FMath::Max(MaxY, FMath::Max(SettlementRewards[i], FactionVal));
    }

    if (FMath::IsNearlyEqual(MinY, MaxY))
    {
        MinY -= 0.5f;
        MaxY += 0.5f;
    }

    const float YRange = MaxY - MinY;

    // Build line arrays
    TArray<FVector2D> SettlementLine;
    TArray<FVector2D> FactionLine;
    SettlementLine.Reserve(NumPoints);
    FactionLine.Reserve(NumPoints);

    for (int32 i = StartIdx; i < SettlementRewards.Num(); ++i)
    {
        const float NormX = static_cast<float>(i - StartIdx) / static_cast<float>(NumPoints - 1);
        const float SettlementNormY = 1.0f - (SettlementRewards[i] - MinY) / YRange;
        SettlementLine.Add(FVector2D(ChartLeft + NormX * ChartWidth, ChartTop + SettlementNormY * ChartHeight));

        if (FactionRewards.IsValidIndex(i))
        {
            const float FactionNormY = 1.0f - (FactionRewards[i] - MinY) / YRange;
            FactionLine.Add(FVector2D(ChartLeft + NormX * ChartWidth, ChartTop + FactionNormY * ChartHeight));
        }
    }

    // Draw lines
    const FLinearColor SettlementColor(0.2f, 0.5f, 1.0f, 1.0f);
    const FLinearColor FactionColor(0.2f, 0.9f, 0.3f, 1.0f);

    FSlateDrawElement::MakeLines(
        OutDrawElements, LayerId + 1,
        AllottedGeometry.ToPaintGeometry(),
        SettlementLine, ESlateDrawEffect::None, SettlementColor, true, 2.0f);

    if (FactionLine.Num() >= 2)
    {
        FSlateDrawElement::MakeLines(
            OutDrawElements, LayerId + 1,
            AllottedGeometry.ToPaintGeometry(),
            FactionLine, ESlateDrawEffect::None, FactionColor, true, 2.0f);
    }

    // Draw axis labels
    const FSlateFontInfo Font = FCoreStyle::Get().GetFontStyle("SmallFont");

    // Y axis: min and max
    FSlateDrawElement::MakeText(
        OutDrawElements, LayerId + 2,
        AllottedGeometry.ToPaintGeometry(FVector2f(Margin, 14.0f), FSlateLayoutTransform(FVector2f(2.0f, ChartBottom - 12.0f))),
        FString::Printf(TEXT("%.2f"), MinY),
        Font, ESlateDrawEffect::None, FLinearColor::White);

    FSlateDrawElement::MakeText(
        OutDrawElements, LayerId + 2,
        AllottedGeometry.ToPaintGeometry(FVector2f(Margin, 14.0f), FSlateLayoutTransform(FVector2f(2.0f, ChartTop))),
        FString::Printf(TEXT("%.2f"), MaxY),
        Font, ESlateDrawEffect::None, FLinearColor::White);

    // X axis: episode range
    FSlateDrawElement::MakeText(
        OutDrawElements, LayerId + 2,
        AllottedGeometry.ToPaintGeometry(FVector2f(80.0f, 14.0f), FSlateLayoutTransform(FVector2f(ChartLeft, ChartBottom + 2.0f))),
        FString::Printf(TEXT("Ep %d"), StartIdx),
        Font, ESlateDrawEffect::None, FLinearColor::White);

    FSlateDrawElement::MakeText(
        OutDrawElements, LayerId + 2,
        AllottedGeometry.ToPaintGeometry(FVector2f(80.0f, 14.0f), FSlateLayoutTransform(FVector2f(ChartRight - 60.0f, ChartBottom + 2.0f))),
        FString::Printf(TEXT("Ep %d"), SettlementRewards.Num()),
        Font, ESlateDrawEffect::None, FLinearColor::White);

    return LayerId + 3;
}
