// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

struct FArcIWMinimapEntry
{
	FVector2D Position;
	FLinearColor Color;
	bool bHydrated = false;
};

struct FArcIWMinimapPartitionBounds
{
	FBox2D Bounds = FBox2D(ForceInit);
	FLinearColor Color;
	TWeakObjectPtr<AActor> PartitionActor;
};

struct FArcIWMinimapGridCell
{
	FBox2D Bounds = FBox2D(ForceInit);
	FLinearColor Color;
};

class SArcIWMinimapTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcIWMinimapTab) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	void RefreshEntityData();
	void FocusAll();

	FVector2D WorldToScreen(const FVector2D& WorldPos, const FVector2D& Center) const;
	FVector2D ScreenToWorld(const FVector2D& ScreenPos, const FVector2D& Center) const;
	FLinearColor GetColorForClass(const FString& ClassName) const;

	void PaintBackground(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void PaintGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void PaintPartitionBounds(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void PaintGridCells(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void PaintEntities(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void PaintLegend(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void HandleLeftClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	FVector2D CameraCenter = FVector2D::ZeroVector;
	float WorldUnitsPerPixel = 100.0f;
	bool bIsPanning = false;
	FVector2D LastMousePos = FVector2D::ZeroVector;

	TArray<FArcIWMinimapEntry> CachedEntities;
	TArray<FArcIWMinimapPartitionBounds> CachedPartitionBounds;
	TArray<FArcIWMinimapGridCell> CachedGridCells;
	TMap<FString, FLinearColor> VisibleClassColors;
	FTimerHandle RefreshTimerHandle;

	bool bShowInstances = true;
	bool bShowBounds = true;

	TSharedPtr<STextBlock> EntityCountText;
};
