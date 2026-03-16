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

#include "GameSettingValueDiscrete.h"

#include "LyraSettingValueDiscrete_Resolution.generated.h"

namespace EWindowMode { enum Type : int; }

class UObject;
struct FScreenResolutionRHI;

UCLASS()
class ULyraSettingValueDiscrete_Resolution : public UGameSettingValueDiscrete
{
	GENERATED_BODY()
	
public:

	ULyraSettingValueDiscrete_Resolution();

	/** UGameSettingValue */
	virtual void StoreInitial() override;
	virtual void ResetToDefault() override;
	virtual void RestoreToInitial() override;

	/** UGameSettingValueDiscrete */
	virtual void SetDiscreteOptionByIndex(int32 Index) override;
	virtual int32 GetDiscreteOptionIndex() const override;
	virtual TArray<FText> GetDiscreteOptions() const override;

protected:
	/** UGameSettingValue */
	virtual void OnInitialized() override;
	virtual void OnDependencyChanged() override;

	void InitializeResolutions();
	bool ShouldAllowFullScreenResolution(const FScreenResolutionRHI& SrcScreenRes, int32 FilterThreshold) const;
	static void GetStandardWindowResolutions(const FIntPoint& MinResolution, const FIntPoint& MaxResolution, float MinAspectRatio, TArray<FIntPoint>& OutResolutions);
	void SelectAppropriateResolutions();
	int32 FindIndexOfDisplayResolution(const FIntPoint& InPoint) const;
	int32 FindIndexOfDisplayResolutionForceValid(const FIntPoint& InPoint) const;
	int32 FindClosestResolutionIndex(const FIntPoint& Resolution) const;

	TOptional<EWindowMode::Type> LastWindowMode;

	struct FScreenResolutionEntry
	{
		uint32	Width = 0;
		uint32	Height = 0;
		uint32	RefreshRate = 0;
		FText   OverrideText;

		FIntPoint GetResolution() const { return FIntPoint(Width, Height); }
		FText GetDisplayText() const;
	};

	/** An array of strings the map to resolutions, populated based on the window mode */
	TArray< TSharedPtr< FScreenResolutionEntry > > Resolutions;

	/** An array of strings the map to fullscreen resolutions */
	TArray< TSharedPtr< FScreenResolutionEntry > > ResolutionsFullscreen;

	/** An array of strings the map to windowed fullscreen resolutions */
	TArray< TSharedPtr< FScreenResolutionEntry > > ResolutionsWindowedFullscreen;

	/** An array of strings the map to windowed resolutions */
	TArray< TSharedPtr< FScreenResolutionEntry > > ResolutionsWindowed;
};
