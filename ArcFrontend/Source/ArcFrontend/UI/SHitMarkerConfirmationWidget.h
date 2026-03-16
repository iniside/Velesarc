/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once


#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/CoreStyle.h"
#include "Engine/LocalPlayer.h"
#include "GameplayTagContainer.h"

struct FLocalPlayerContext;

class SHitMarkerConfirmationWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SHitMarkerConfirmationWidget)
		: _PerHitMarkerImage(FCoreStyle::Get().GetBrush("Throbber.CircleChunk"))
		, _AnyHitsMarkerImage(nullptr)
		, _HitNotifyDuration(0.4f)
	{
	}
		/** The marker image to draw for individual hit markers. */
		SLATE_ARGUMENT(const FSlateBrush*, PerHitMarkerImage)
		/** The marker image to draw if there are any hits at all. */
		SLATE_ARGUMENT(const FSlateBrush*, AnyHitsMarkerImage)
		/** The duration (in seconds) to display hit notifies (they fade to transparent over this time)  */
		SLATE_ATTRIBUTE(float, HitNotifyDuration)
		/** The color and opacity of the marker */
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InContext, const TMap<FGameplayTag, FSlateBrush>& ZoneOverrideImages);

	SHitMarkerConfirmationWidget();

	//~SWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual bool ComputeVolatility() const override { return true; }
	//~End of SWidget interface

private:
	/** The marker image to draw for individual hit markers. */
	const FSlateBrush* PerHitMarkerImage = nullptr;

	/** Map from zone tag (e.g., weak spot) to override marker images. */
	TMap<FGameplayTag, FSlateBrush> PerHitMarkerZoneOverrideImages;

	/** The marker image to draw if there are any hits at all. */
	const FSlateBrush* AnyHitsMarkerImage = nullptr;

	/** The opacity for the hit markers */
	float HitNotifyOpacity = 0.0f;

	/** The duration (in seconds) to display hit notifies (they fade to transparent over this time)  */
	float HitNotifyDuration = 0.4f;

	/** Color and opacity of the markers */
	TAttribute<FSlateColor> ColorAndOpacity;
	bool bColorAndOpacitySet;

	/** Player context for the owning HUD */
	FLocalPlayerContext MyContext;
};
