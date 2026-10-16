// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Layout/SlateRect.h"

namespace ArcRmlContentRect
{
	// All values are absolute pixels. A MaxAspect of 0 means no limit.
	ARCRMLUI_API FSlateRect Compute(const FSlateRect& ViewportRect, const FMargin& SafeMargin, float MaxAspect);

	ARCRMLUI_API FMargin GetSafeMargin();
}
