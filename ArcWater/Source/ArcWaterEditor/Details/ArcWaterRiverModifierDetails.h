// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Types/SlateEnums.h"

class UArcWaterRiverModifier;

class FArcWaterRiverModifierDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply RebuildRivers();
	FReply ClearRivers();
	FReply RefreshRiverPreview();
	void SetRiverPreviewVisible(ECheckBoxState CheckState);
	void SetRiverPreviewThroughTerrain(ECheckBoxState CheckState);
	ECheckBoxState GetRiverPreviewVisibilityState() const;
	ECheckBoxState GetRiverPreviewThroughTerrainState() const;
	bool CanRebuildRivers() const;
	FText GetResultText() const;
	FText GetErrorText() const;
	EVisibility GetErrorVisibility() const;
	void RefreshResultStats();

	TWeakObjectPtr<UArcWaterRiverModifier> Modifier;
	FText LastError;
	int32 SourceComponentCount = 0;
	int32 ReachCount = 0;
	int32 JunctionCount = 0;
	int32 SectionCount = 0;
	double TotalLengthCm = 0.0;
	int32 SplineModifierCount = 0;
	FIntPoint MaxFieldDims = FIntPoint::ZeroValue;
	double FieldSpacingCm = 0.0;
	int64 EstimatedRasterBytes = 0;
};
