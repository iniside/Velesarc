// Copyright Lukasz Baran. All Rights Reserved.

#include "Host/ArcRmlContentRect.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SViewport.h"

FSlateRect ArcRmlContentRect::Compute(const FSlateRect& ViewportRect, const FMargin& SafeMargin, float MaxAspect)
{
	float Left = ViewportRect.Left + SafeMargin.Left;
	float Right = ViewportRect.Right - SafeMargin.Right;
	const float Top = ViewportRect.Top + SafeMargin.Top;
	const float Bottom = ViewportRect.Bottom - SafeMargin.Bottom;

	const float MaxWidth = MaxAspect * (Bottom - Top);
	if (MaxAspect > 0.0f && Right - Left > MaxWidth)
	{
		Left = (Left + Right - MaxWidth) * 0.5f;
		Right = Left + MaxWidth;
	}

	return FSlateRect(FMath::RoundToFloat(Left), FMath::RoundToFloat(Top), FMath::RoundToFloat(Right), FMath::RoundToFloat(Bottom));
}

FMargin ArcRmlContentRect::GetSafeMargin()
{
	FMargin SafeMargin;
#if WITH_EDITOR
	const TSharedPtr<SViewport> GameViewport = FSlateApplication::Get().GetGameViewport();
	const TSharedPtr<ISlateViewport> ViewportInterface = GameViewport.IsValid() ? GameViewport->GetViewportInterface().Pin() : nullptr;
	if (!ViewportInterface.IsValid())
	{
		return SafeMargin;
	}
	FSlateApplication::Get().GetSafeZoneSize(SafeMargin, ViewportInterface->GetSize());
#else
	FSlateApplication::Get().GetSafeZoneSize(SafeMargin, FVector2f::ZeroVector);
#endif

	const TOptional<float> GlobalScale = SSafeZone::GetGlobalSafeZoneScale();
	if (GlobalScale.IsSet())
	{
		SafeMargin = SafeMargin * GlobalScale.GetValue();
	}
	return SafeMargin;
}
