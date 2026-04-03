// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcCompositeMeshPreviewContributor.h"

#include "AdvancedPreviewScene.h"
#include "GameFramework/Actor.h"
#include "ArcMass/CompositeVisualization/ArcCompositeMeshVisualizationTrait.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Math/Box.h"
// ---- Composite Mesh Preview Contributor ----

bool FArcCompositeMeshPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcCompositeMeshVisualizationTrait* CompositeTrait = Cast<UArcCompositeMeshVisualizationTrait>(Trait);
	return CompositeTrait && CompositeTrait->IsValid();
}

void FArcCompositeMeshPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor)
{
	CachedTrait = const_cast<UArcCompositeMeshVisualizationTrait*>(CastChecked<UArcCompositeMeshVisualizationTrait>(Trait));

	const TArray<FArcCompositeMeshEntry>& Entries = CachedTrait->GetMeshEntries();

	while (PreviewComponents.Num() < Entries.Num())
	{
		UStaticMeshComponent* NewComp = NewObject<UStaticMeshComponent>(PreviewActor);
		PreviewActor->AddOwnedComponent(NewComp);
		Scene.AddComponent(NewComp, FTransform::Identity);
		PreviewComponents.Add(NewComp);
	}

	while (PreviewComponents.Num() > Entries.Num())
	{
		UStaticMeshComponent* Comp = PreviewComponents.Pop();
		Scene.RemoveComponent(Comp);
	}

	for (int32 Idx = 0; Idx < Entries.Num(); ++Idx)
	{
		const FArcCompositeMeshEntry& Entry = Entries[Idx];
		UStaticMeshComponent* Comp = PreviewComponents[Idx];

		Comp->SetStaticMesh(Entry.Mesh);
		Comp->SetRelativeTransform(Entry.RelativeTransform);

		for (int32 MatIdx = 0; MatIdx < Entry.MaterialOverrides.Num(); ++MatIdx)
		{
			Comp->SetMaterial(MatIdx, Entry.MaterialOverrides[MatIdx]);
		}

		Comp->SetVisibility(Entry.Mesh != nullptr);
		Comp->MarkRenderStateDirty();
	}
}

void FArcCompositeMeshPreviewContributor::Clear(FAdvancedPreviewScene& Scene)
{
	for (UStaticMeshComponent* Comp : PreviewComponents)
	{
		if (Comp)
		{
			Scene.RemoveComponent(Comp);
		}
	}
	PreviewComponents.Empty();
	CachedTrait = nullptr;
	SelectedMeshIndex = INDEX_NONE;
}

FBox FArcCompositeMeshPreviewContributor::GetBounds() const
{
	FBox CombinedBox(ForceInit);
	for (UStaticMeshComponent* Comp : PreviewComponents)
	{
		if (Comp && Comp->GetStaticMesh())
		{
			CombinedBox += Comp->Bounds.GetBox();
		}
	}
	return CombinedBox;
}

int32 FArcCompositeMeshPreviewContributor::GetSelectableCount() const
{
	return PreviewComponents.Num();
}

FTransform FArcCompositeMeshPreviewContributor::GetSelectableTransform(int32 Index) const
{
	if (PreviewComponents.IsValidIndex(Index) && PreviewComponents[Index])
	{
		return PreviewComponents[Index]->GetRelativeTransform();
	}
	return FTransform::Identity;
}

void FArcCompositeMeshPreviewContributor::SetSelectableTransform(int32 Index, const FTransform& NewTransform)
{
	if (CachedTrait && PreviewComponents.IsValidIndex(Index) && PreviewComponents[Index])
	{
		CachedTrait->SetMeshEntryTransform(Index, NewTransform);
		CachedTrait->MarkPackageDirty();
		PreviewComponents[Index]->SetRelativeTransform(NewTransform);
		PreviewComponents[Index]->MarkRenderStateDirty();
	}
}

UObject* FArcCompositeMeshPreviewContributor::GetMutableTraitObject() const
{
	return CachedTrait;
}

int32 FArcCompositeMeshPreviewContributor::GetPreviewPrimitiveCount() const
{
	return PreviewComponents.Num();
}

UPrimitiveComponent* FArcCompositeMeshPreviewContributor::GetPreviewPrimitive(int32 Index) const
{
	if (PreviewComponents.IsValidIndex(Index))
	{
		return PreviewComponents[Index];
	}
	return nullptr;
}

void FArcCompositeMeshPreviewContributor::SetSelectionHighlight(int32 Index)
{
	if (SelectedMeshIndex != INDEX_NONE && PreviewComponents.IsValidIndex(SelectedMeshIndex))
	{
		PreviewComponents[SelectedMeshIndex]->SetRenderCustomDepth(false);
	}

	SelectedMeshIndex = Index;

	if (SelectedMeshIndex != INDEX_NONE && PreviewComponents.IsValidIndex(SelectedMeshIndex))
	{
		PreviewComponents[SelectedMeshIndex]->SetRenderCustomDepth(true);
		PreviewComponents[SelectedMeshIndex]->SetCustomDepthStencilValue(1);
	}
}

