// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSDebuggerDrawComponent.h"
#include "TargetQuery/ArcTQSQuerySubsystem.h"

#if UE_ENABLE_DEBUG_DRAWING
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#endif

namespace ArcTQSDebuggerDraw
{
	static FColor ScoreToGradient(float Score)
	{
		Score = FMath::Clamp(Score, 0.0f, 1.0f);
		if (Score < 0.5f)
		{
			const float T = Score * 2.0f;
			return FColor(
				static_cast<uint8>(50 * (1.0f - T)),
				static_cast<uint8>(100 + 120 * T),
				static_cast<uint8>(255 - 55 * T));
		}
		else
		{
			const float T = (Score - 0.5f) * 2.0f;
			return FColor(
				0,
				static_cast<uint8>(220 + 35 * T),
				static_cast<uint8>(200 - 150 * T));
		}
	}

	static const FColor ResultColor(255, 50, 50);
	static const FColor SelectedColor = FColor::White;
	static const FColor FilteredColor(128, 128, 128);
}

// --- Delegate Helper ---

void FArcTQSDebugDrawDelegateHelper_Debugger::SetupFromProxy(const FDebugRenderSceneProxy* InSceneProxy, bool bInDrawLabels)
{
	Super::InitDelegateHelper(InSceneProxy);
	bDrawLabels = bInDrawLabels;
}

void FArcTQSDebugDrawDelegateHelper_Debugger::DrawDebugLabels(UCanvas* Canvas, APlayerController* PC)
{
	if (bDrawLabels)
	{
		Super::DrawDebugLabels(Canvas, PC);
	}
}

// --- Draw Component ---

UArcTQSDebuggerDrawComponent::UArcTQSDebuggerDrawComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsVisualizationComponent(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bSelectable = false;
}

void UArcTQSDebuggerDrawComponent::UpdateQueryData(
	const FArcTQSDebugQueryData& InData,
	bool bInDrawAllItems,
	bool bInDrawLabels,
	bool bInDrawScores,
	bool bInDrawFilteredItems,
	int32 InSelectedItemIndex,
	int32 InHoveredItemIndex)
{
	StoredSpheres.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	bStoredDrawLabels = bInDrawLabels || bInDrawScores;

	CollectShapes(InData, bInDrawAllItems, bInDrawLabels, bInDrawScores, bInDrawFilteredItems, InSelectedItemIndex, InHoveredItemIndex, StoredSpheres, StoredTexts, StoredArrows);
	MarkRenderStateDirty();
}

void UArcTQSDebuggerDrawComponent::ClearQueryData()
{
	StoredSpheres.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	MarkRenderStateDirty();
}

FBoxSphereBounds UArcTQSDebuggerDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);
	for (const FDebugRenderSceneProxy::FSphere& Sphere : StoredSpheres)
	{
		BoundingBox += FBox::BuildAABB(Sphere.Location, FVector(Sphere.Radius));
	}
	if (!BoundingBox.IsValid)
	{
		BoundingBox = FBox(FVector::ZeroVector, FVector(100.0f));
	}
	return FBoxSphereBounds(BoundingBox);
}

#if UE_ENABLE_DEBUG_DRAWING
FDebugRenderSceneProxy* UArcTQSDebuggerDrawComponent::CreateDebugSceneProxy()
{
	FDebugRenderSceneProxy* Proxy = new FDebugRenderSceneProxy(this);

	Proxy->Spheres = StoredSpheres;
	Proxy->Texts = StoredTexts;
	Proxy->ArrowLines = StoredArrows;

	DebugDrawDelegateHelper.Reset();
	DebugDrawDelegateHelper.SetupFromProxy(Proxy, bStoredDrawLabels);

	return Proxy;
}
#endif

void UArcTQSDebuggerDrawComponent::CollectShapes(
	const FArcTQSDebugQueryData& Data,
	bool bDrawAllItems,
	bool bDrawLabels,
	bool bDrawScores,
	bool bDrawFilteredItems,
	int32 SelectedIndex,
	int32 HoveredIndex,
	TArray<FDebugRenderSceneProxy::FSphere>& OutSpheres,
	TArray<FDebugRenderSceneProxy::FText3d>& OutTexts,
	TArray<FDebugRenderSceneProxy::FArrowLine>& OutArrows) const
{
	const FVector QuerierLoc = Data.QuerierLocation;

	// Querier marker
	OutSpheres.Emplace(40.0f, QuerierLoc, FColor::Blue);

	if (bDrawLabels)
	{
		OutTexts.Emplace(
			FString::Printf(TEXT("Querier  Gen:%d Valid:%d Res:%d  %.2fms"),
				Data.TotalGenerated, Data.TotalValid,
				Data.Results.Num(), Data.ExecutionTimeMs),
			QuerierLoc + FVector(0, 0, 80.0f),
			FColor::White);
	}

	// Context locations
	for (int32 i = 0; i < Data.ContextLocations.Num(); ++i)
	{
		const FVector& CtxLoc = Data.ContextLocations[i];
		if (!CtxLoc.Equals(QuerierLoc, 10.0f))
		{
			OutSpheres.Emplace(25.0f, CtxLoc, FColor::Yellow);
			OutArrows.Emplace(QuerierLoc, CtxLoc, FColor::Yellow);

			if (bDrawLabels)
			{
				OutTexts.Emplace(
					FString::Printf(TEXT("Ctx %d"), i),
					CtxLoc + FVector(0, 0, 50.0f),
					FColor::Yellow);
			}
		}
	}

	// Build result item-to-R-index map
	TMap<int32, int32> ItemToResultIndex;
	for (int32 ResultIdx = 0; ResultIdx < Data.Results.Num(); ++ResultIdx)
	{
		const FArcTQSTargetItem& Result = Data.Results[ResultIdx];
		for (int32 i = 0; i < Data.AllItems.Num(); ++i)
		{
			if (Data.AllItems[i].Location.Equals(Result.Location, 1.0f) &&
				Data.AllItems[i].TargetType == Result.TargetType)
			{
				ItemToResultIndex.Add(i, ResultIdx);
				break;
			}
		}
	}

	// Draw items
	for (int32 i = 0; i < Data.AllItems.Num(); ++i)
	{
		const FArcTQSTargetItem& Item = Data.AllItems[i];
		const FVector ItemLoc = Item.Location;
		const bool bIsResult = ItemToResultIndex.Contains(i);
		const bool bIsSelected = (i == SelectedIndex);
		const bool bIsHovered = (i == HoveredIndex);

		if (!bDrawAllItems && !bIsSelected && !bIsHovered)
		{
			continue;
		}

		if (!Item.bValid)
		{
			if (!bDrawFilteredItems)
			{
				continue;
			}

			float Radius = 6.0f;
			FColor DrawColor = ArcTQSDebuggerDraw::FilteredColor;
			if (bIsSelected)
			{
				Radius = 45.0f;
				DrawColor = ArcTQSDebuggerDraw::SelectedColor;
			}
			else if (bIsHovered)
			{
				Radius = 20.0f;
			}

			OutSpheres.Emplace(Radius, ItemLoc + FVector(0, 0, 15.0f), DrawColor);

			if (bDrawScores || bIsSelected || bIsHovered)
			{
				OutTexts.Emplace(
					FString::Printf(TEXT("[%d] filtered"), i),
					ItemLoc + FVector(0, 0, 50.0f),
					DrawColor);
			}
			continue;
		}

		if (bIsResult)
		{
			const int32 RIdx = ItemToResultIndex[i];
			float Radius = 25.0f;
			FColor DrawColor = ArcTQSDebuggerDraw::ResultColor;
			if (bIsSelected)
			{
				Radius = 45.0f;
				DrawColor = ArcTQSDebuggerDraw::SelectedColor;
			}
			else if (bIsHovered)
			{
				Radius = 35.0f;
			}

			OutSpheres.Emplace(Radius, ItemLoc, DrawColor);
			OutArrows.Emplace(QuerierLoc, ItemLoc, ArcTQSDebuggerDraw::ResultColor);

			if (bDrawScores || bIsSelected || bIsHovered)
			{
				OutTexts.Emplace(
					FString::Printf(TEXT("[R%d] %.3f"), RIdx, Item.Score),
					ItemLoc + FVector(0, 0, 50.0f),
					DrawColor);
			}
		}
		else
		{
			const FColor GradientColor = ArcTQSDebuggerDraw::ScoreToGradient(Item.Score);
			float Radius = 12.0f;
			FColor DrawColor = GradientColor;
			if (bIsSelected)
			{
				Radius = 45.0f;
				DrawColor = ArcTQSDebuggerDraw::SelectedColor;
			}
			else if (bIsHovered)
			{
				Radius = 25.0f;
			}

			OutSpheres.Emplace(Radius, ItemLoc, DrawColor);

			if (bDrawScores || bIsSelected || bIsHovered)
			{
				OutTexts.Emplace(
					FString::Printf(TEXT("[%d] %.3f"), i, Item.Score),
					ItemLoc + FVector(0, 0, 50.0f),
					DrawColor);
			}
		}
	}
}