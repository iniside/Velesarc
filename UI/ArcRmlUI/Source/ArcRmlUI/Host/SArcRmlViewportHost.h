// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Widgets/SCompoundWidget.h"

class SArcRmlViewport;

// The widget pool builds Slate before the container's InitFunc assigns the document, so the viewport arrives after the host exists.
class SArcRmlViewportHost final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcRmlViewportHost)
	{
	}
		SLATE_ATTRIBUTE(float, MaxContentAspect)
	SLATE_END_ARGS()

	virtual ~SArcRmlViewportHost() override;

	void Construct(const FArguments& InArgs);

	void SetViewport(const TSharedPtr<SArcRmlViewport>& InViewport);
	bool HasViewport() const;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;

private:
	void MarkSafeMarginDirty();
#if WITH_EDITOR
	void HandleDebugSafeZoneChanged(const FMargin& NewSafeZone, bool bShouldRecacheMetrics);
#endif

	TWeakPtr<SArcRmlViewport> Viewport;
	TAttribute<float> MaxContentAspect;
	FDelegateHandle SafeFrameChangedHandle;
#if WITH_EDITOR
	FDelegateHandle DebugSafeZoneChangedHandle;
#endif
	FMargin SafeMargin;
	FVector2f SafeMarginHostSize = FVector2f::ZeroVector;
	bool bSafeMarginDirty = true;
};
