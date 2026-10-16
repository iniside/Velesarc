// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSRenderingComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace ArcTQSRenderDebug
{
	// Lerp from red (0.0) through yellow (0.5) to green (1.0)
	static FColor ScoreToColor(float Score)
	{
		Score = FMath::Clamp(Score, 0.0f, 1.0f);
		if (Score < 0.5f)
		{
			const float T = Score * 2.0f;
			return FColor(255, static_cast<uint8>(T * 255), 0);
		}
		else
		{
			const float T = (Score - 0.5f) * 2.0f;
			return FColor(static_cast<uint8>((1.0f - T) * 255), 255, 0);
		}
	}
}

// --------------------------------------------------------------------------
// FArcTQSDebugDrawDelegateHelper
// --------------------------------------------------------------------------

void FArcTQSDebugDrawDelegateHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController* PC)
{
	if (!bDrawLabels)
	{
		return;
	}

	FDebugDrawDelegateHelper::DrawDebugLabels(Canvas, PC);
}

class FArcTQSDebugDrawSceneProxy final : public FDebugRenderSceneProxy
{
public:
	explicit FArcTQSDebugDrawSceneProxy(const UPrimitiveComponent& InComponent, const EDrawType InDrawType = EDrawType::SolidAndWireMeshes, const TCHAR* InViewFlagName = nullptr)
	: FDebugRenderSceneProxy(&InComponent)
	{
		DrawType = InDrawType;
		ViewFlagName = InViewFlagName;
		ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*ViewFlagName));
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = true;
		Result.bDynamicRelevance = true;
		Result.bSeparateTranslucency = Result.bNormalTranslucency = true;
		return Result;
	}
	
	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + FDebugRenderSceneProxy::GetAllocatedSize();
	}

private:
	uint32 ViewFlagIndex = 0;
};

// --------------------------------------------------------------------------
// UArcTQSRenderingComponent
// --------------------------------------------------------------------------

UArcTQSRenderingComponent::UArcTQSRenderingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UArcTQSRenderingComponent::UpdateQueryData(
	const FArcTQSQueryResult& InResult,
	TConstArrayView<FArcTQSRenderMarker> Markers,
	bool bInDrawLabels,
	bool bInDrawFilteredItems)
{
	StoredSpheres.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	bStoredDrawLabels = bInDrawLabels;

	CollectShapes(InResult, Markers, bInDrawLabels, bInDrawFilteredItems,
		StoredSpheres, StoredTexts, StoredArrows);

	MarkRenderStateDirty();
}

void UArcTQSRenderingComponent::ClearQueryData()
{
	StoredSpheres.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();

#if UE_ENABLE_DEBUG_DRAWING
	TQSDebugDrawDelegateHelper.Reset();
#endif

	MarkRenderStateDirty();
}

FBoxSphereBounds UArcTQSRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);

	for (const auto& Sphere : StoredSpheres)
	{
		BoundingBox += FBox::BuildAABB(FVector(Sphere.Location), FVector(Sphere.Radius));
	}

	// If no shapes, use a large default bound so the component is always visible
	if (!BoundingBox.IsValid)
	{
		BoundingBox = FBox(FVector(-5000.0f), FVector(5000.0f));
	}

	return FBoxSphereBounds(BoundingBox);
}

#if UE_ENABLE_DEBUG_DRAWING
FDebugRenderSceneProxy* UArcTQSRenderingComponent::CreateDebugSceneProxy()
{
	FArcTQSDebugDrawSceneProxy* Proxy = new FArcTQSDebugDrawSceneProxy(*this, FDebugRenderSceneProxy::SolidAndWireMeshes, TEXT("DebugAI"));
	Proxy->DrawType = FDebugRenderSceneProxy::SolidAndWireMeshes;
	Proxy->TextWithoutShadowDistance = 1500.0f;

	Proxy->Spheres.Append(StoredSpheres);
	Proxy->Texts.Append(StoredTexts);
	Proxy->ArrowLines.Append(StoredArrows);

	TQSDebugDrawDelegateHelper.SetDrawLabels(bStoredDrawLabels);

	return Proxy;
}
#endif

void UArcTQSRenderingComponent::CollectShapes(
	const FArcTQSQueryResult& Result,
	TConstArrayView<FArcTQSRenderMarker> Markers,
	bool bDrawLabels, bool bDrawFilteredItems,
	TArray<FDebugRenderSceneProxy::FSphere>& OutSpheres,
	TArray<FDebugRenderSceneProxy::FText3d>& OutTexts,
	TArray<FDebugRenderSceneProxy::FArrowLine>& OutArrows) const
{
	const FVector QuerierLoc = Result.QuerierLocation;

	OutSpheres.Emplace(40.0f, QuerierLoc, FColor::White);

	if (bDrawLabels)
	{
		OutTexts.Emplace(
			FString::Printf(TEXT("Gen:%d Valid:%d Res:%d  %.2fms"),
				Result.TotalGenerated, Result.TotalValid,
				Result.ResultIndices.Num(), Result.ExecutionTimeMs),
			QuerierLoc + FVector(0, 0, 80.0f),
			FColor::White);
	}

#if !UE_BUILD_SHIPPING
	if (bDrawLabels)
	{
		TArray<FString> LevelOnlyNodes;
		if (Result.bGeneratorLevelOnly)
		{
			LevelOnlyNodes.Add(TEXT("Generator"));
		}
		for (const FArcTQSStepResult& Step : Result.Steps)
		{
			if (Step.State == EArcTQSStepState::Skipped)
			{
				LevelOnlyNodes.Add(Step.StepName);
			}
		}
		if (LevelOnlyNodes.Num() > 0)
		{
			OutTexts.Emplace(
				FString::Printf(TEXT("Level only: %s"), *FString::Join(LevelOnlyNodes, TEXT(", "))),
				QuerierLoc + FVector(0, 0, 110.0f),
				FColor::Orange);
		}
	}
#endif

	for (const FArcTQSRenderMarker& Marker : Markers)
	{
		OutSpheres.Emplace(25.0f, Marker.Location, FColor::Magenta);
		if (bDrawLabels && !Marker.Name.IsEmpty())
		{
			OutTexts.Emplace(Marker.Name, Marker.Location + FVector(0, 0, 45.0f), FColor::Magenta);
		}
	}

	for (int32 i = 0; i < Result.Items.Num(); ++i)
	{
		const FArcTQSTargetItem& Item = Result.Items[i];

		if (!Item.bValid)
		{
			if (bDrawFilteredItems)
			{
				OutSpheres.Emplace(6.0f, Item.Location + FVector(0, 0, 15.0f), FColor(80, 80, 80));
#if !UE_BUILD_SHIPPING
				const int32 FilteredBy = Result.FilteredByStep.IsValidIndex(i) ? Result.FilteredByStep[i] : INDEX_NONE;
				if (bDrawLabels && Result.Steps.IsValidIndex(FilteredBy))
				{
					OutTexts.Emplace(
						Result.Steps[FilteredBy].StepName,
						Item.Location + FVector(0, 0, 30.0f),
						FColor(128, 128, 128));
				}
#endif
			}
			continue;
		}

		const bool bIsResult = Result.ResultIndices.Contains(i);
		const FColor Color = ArcTQSRenderDebug::ScoreToColor(Item.Score);

		if (bIsResult)
		{
			OutSpheres.Emplace(30.0f, Item.Location, Color);

			OutArrows.Emplace(
				FVector(QuerierLoc + FVector(0, 0, 60.0f)),
				FVector(Item.Location + FVector(0, 0, 60.0f)),
				FColor::Cyan);

			if (bDrawLabels)
			{
				OutTexts.Emplace(
					FString::Printf(TEXT("%.3f"), Item.Score),
					Item.Location + FVector(0, 0, 50.0f),
					Color);
			}
		}
		else
		{
			OutSpheres.Emplace(15.0f, Item.Location, Color);

			if (bDrawLabels)
			{
				OutTexts.Emplace(
					FString::Printf(TEXT("%.3f"), Item.Score),
					Item.Location + FVector(0, 0, 30.0f),
					Color);
			}
		}
	}
}
