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

void FArcTQSDebugDrawDelegateHelper::SetupFromProxy(
	const FDebugRenderSceneProxy* InSceneProxy, AActor* InActorOwner, bool bInDrawLabels)
{
	ActorOwner = InActorOwner;
	bDrawLabels = bInDrawLabels;
}

void FArcTQSDebugDrawDelegateHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController* PC)
{
	if (!ActorOwner || !bDrawLabels)
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
	const FArcTQSDebugQueryData& InData, bool bInDrawLabels, bool bInDrawFilteredItems)
{
	StoredSpheres.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	bStoredDrawLabels = bInDrawLabels;

	CollectShapes(InData, bInDrawLabels, bInDrawFilteredItems,
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

	TQSDebugDrawDelegateHelper.SetupFromProxy(Proxy, GetOwner(), bStoredDrawLabels);

	return Proxy;
}
#endif

void UArcTQSRenderingComponent::CollectShapes(
	const FArcTQSDebugQueryData& Data,
	bool bDrawLabels, bool bDrawFilteredItems,
	TArray<FDebugRenderSceneProxy::FSphere>& OutSpheres,
	TArray<FDebugRenderSceneProxy::FText3d>& OutTexts,
	TArray<FDebugRenderSceneProxy::FArrowLine>& OutArrows) const
{
	const FVector QuerierLoc = Data.QuerierLocation;

	// --- Querier marker ---
	OutSpheres.Emplace(40.0f, QuerierLoc, FColor::White);

	if (bDrawLabels)
	{
		OutTexts.Emplace(
			FString::Printf(TEXT("Gen:%d Valid:%d Res:%d  %.2fms"),
				Data.TotalGenerated, Data.TotalValid,
				Data.Results.Num(), Data.ExecutionTimeMs),
			QuerierLoc + FVector(0, 0, 80.0f),
			FColor::White);
	}

	// --- Context locations ---
	for (int32 i = 0; i < Data.ContextLocations.Num(); ++i)
	{
		const FVector& CtxLoc = Data.ContextLocations[i];
		if (!CtxLoc.Equals(QuerierLoc, 10.0f))
		{
			OutSpheres.Emplace(30.0f, CtxLoc, FColor::Orange);
			if (bDrawLabels)
			{
				OutTexts.Emplace(
					FString::Printf(TEXT("Ctx %d"), i),
					CtxLoc + FVector(0, 0, 50.0f),
					FColor::Orange);
			}
		}
	}

	// --- Build result set for quick lookup ---
	TSet<int32> ResultIndices;
	for (const FArcTQSTargetItem& Result : Data.Results)
	{
		for (int32 i = 0; i < Data.AllItems.Num(); ++i)
		{
			if (Data.AllItems[i].Location.Equals(Result.Location, 1.0f) &&
				Data.AllItems[i].TargetType == Result.TargetType)
			{
				ResultIndices.Add(i);
				break;
			}
		}
	}

	// --- Draw all items ---
	for (int32 i = 0; i < Data.AllItems.Num(); ++i)
	{
		const FArcTQSTargetItem& Item = Data.AllItems[i];

		if (!Item.bValid)
		{
			// Filtered-out items: small grey sphere
			if (bDrawFilteredItems)
			{
				OutSpheres.Emplace(6.0f, Item.Location + FVector(0, 0, 15.0f), FColor(80, 80, 80));
			}
			continue;
		}

		const bool bIsResult = ResultIndices.Contains(i);
		const FColor Color = ArcTQSRenderDebug::ScoreToColor(Item.Score);

		if (bIsResult)
		{
			// Result: larger sphere + arrow from querier
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
			// Valid but not selected: small sphere
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
