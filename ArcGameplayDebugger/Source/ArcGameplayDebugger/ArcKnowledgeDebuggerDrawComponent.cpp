// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeDebuggerDrawComponent.h"

#if UE_ENABLE_DEBUG_DRAWING
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "SceneManagement.h"
#endif

namespace ArcKnowledgeDebuggerDraw
{
	static const FColor KnowledgeColor(50, 180, 220);
	static const FColor SelectedColor = FColor::Yellow;
	static const FColor ClaimedColor = FColor::Orange;
	static const FColor RadiusColor(80, 180, 220, 80);
}

// --- Always-visible scene proxy ---

#if UE_ENABLE_DEBUG_DRAWING
class FArcKnowledgeDebuggerSceneProxy final : public FDebugRenderSceneProxy
{
public:
	explicit FArcKnowledgeDebuggerSceneProxy(const UPrimitiveComponent& InComponent)
		: FDebugRenderSceneProxy(&InComponent)
	{
		DrawType = SolidAndWireMeshes;
		TextWithoutShadowDistance = 1500.0f;
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
};
#endif

// --- Delegate Helper ---

void FArcKnowledgeDebugDrawDelegateHelper::SetupFromProxy(const FDebugRenderSceneProxy* InSceneProxy, bool bInDrawLabels)
{
	Super::InitDelegateHelper(InSceneProxy);
	bDrawLabels = bInDrawLabels;
}

void FArcKnowledgeDebugDrawDelegateHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController* PC)
{
	if (bDrawLabels)
	{
		Super::DrawDebugLabels(Canvas, PC);
	}
}

// --- Draw Component ---

UArcKnowledgeDebuggerDrawComponent::UArcKnowledgeDebuggerDrawComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsVisualizationComponent(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bSelectable = false;
}

void UArcKnowledgeDebuggerDrawComponent::UpdateEntryData(
	TArrayView<const FArcKnowledgeDebugDrawEntry> InEntries,
	int32 InSelectedIndex,
	bool bInDrawAllEntries,
	bool bInDrawLabels,
	bool bInDrawSelectedRadius)
{
	StoredSpheres.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	bStoredDrawLabels = bInDrawLabels;

	CollectShapes(InEntries, InSelectedIndex, bInDrawAllEntries, bInDrawLabels, bInDrawSelectedRadius, StoredSpheres, StoredTexts, StoredArrows);
	UpdateBounds();
	MarkRenderStateDirty();
}

void UArcKnowledgeDebuggerDrawComponent::ClearEntryData()
{
	StoredSpheres.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	MarkRenderStateDirty();
}

FBoxSphereBounds UArcKnowledgeDebuggerDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);
	for (const FDebugRenderSceneProxy::FSphere& Sphere : StoredSpheres)
	{
		BoundingBox += FBox::BuildAABB(Sphere.Location, FVector(Sphere.Radius));
	}
	if (!BoundingBox.IsValid)
	{
		BoundingBox = FBox(FVector(-5000.0f), FVector(5000.0f));
	}
	return FBoxSphereBounds(BoundingBox);
}

#if UE_ENABLE_DEBUG_DRAWING
FDebugRenderSceneProxy* UArcKnowledgeDebuggerDrawComponent::CreateDebugSceneProxy()
{
	FArcKnowledgeDebuggerSceneProxy* Proxy = new FArcKnowledgeDebuggerSceneProxy(*this);

	Proxy->Spheres = StoredSpheres;
	Proxy->Texts = StoredTexts;
	Proxy->ArrowLines = StoredArrows;

	DebugDrawDelegateHelper.Reset();
	DebugDrawDelegateHelper.SetupFromProxy(Proxy, bStoredDrawLabels);

	return Proxy;
}
#endif

void UArcKnowledgeDebuggerDrawComponent::CollectShapes(
	TArrayView<const FArcKnowledgeDebugDrawEntry> Entries,
	int32 SelectedIndex,
	bool bDrawAllEntries,
	bool bDrawLabels,
	bool bDrawSelectedRadius,
	TArray<FDebugRenderSceneProxy::FSphere>& OutSpheres,
	TArray<FDebugRenderSceneProxy::FText3d>& OutTexts,
	TArray<FDebugRenderSceneProxy::FArrowLine>& OutArrows) const
{
	constexpr float ZOffset = 80.f;

	// Draw all entry markers
	if (bDrawAllEntries)
	{
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			if (i == SelectedIndex)
			{
				continue; // Selected drawn separately
			}

			const FArcKnowledgeDebugDrawEntry& Entry = Entries[i];
			FColor MarkerColor = Entry.bClaimed
				? ArcKnowledgeDebuggerDraw::ClaimedColor
				: ArcKnowledgeDebuggerDraw::KnowledgeColor;
			float Alpha = FMath::Clamp(Entry.Relevance, 0.3f, 1.0f);
			MarkerColor.A = static_cast<uint8>(Alpha * 255.f);

			OutSpheres.Emplace(10.f, Entry.Location + FVector(0, 0, ZOffset), MarkerColor);

			if (bDrawLabels)
			{
				OutTexts.Emplace(Entry.Label, Entry.Location + FVector(0, 0, ZOffset + 30.f), MarkerColor);
			}
		}
	}

	// Draw selected entry with full detail
	if (SelectedIndex != INDEX_NONE && Entries.IsValidIndex(SelectedIndex))
	{
		const FArcKnowledgeDebugDrawEntry& SelEntry = Entries[SelectedIndex];
		const FVector Loc = SelEntry.Location;

		// Sphere marker
		OutSpheres.Emplace(40.f, Loc + FVector(0, 0, ZOffset), ArcKnowledgeDebuggerDraw::SelectedColor);

		// Detail label
		OutTexts.Emplace(SelEntry.Label, Loc + FVector(0, 0, ZOffset + 60.f), FColor::White);

		// Vertical line
		OutArrows.Emplace(Loc, Loc + FVector(0, 0, ZOffset + 40.f), ArcKnowledgeDebuggerDraw::SelectedColor);

		// Broadcast radius
		if (bDrawSelectedRadius && SelEntry.BroadcastRadius > 0.f)
		{
			OutSpheres.Emplace(SelEntry.BroadcastRadius, Loc, ArcKnowledgeDebuggerDraw::RadiusColor);
		}
	}
}
