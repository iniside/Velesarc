// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcUnifiedMeshVisualizationPreviewContributor.h"

#include "AdvancedPreviewScene.h"
#include "GameFramework/Actor.h"
#include "ArcMass/Visualization/ArcUnifiedMeshVisualizationTrait.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Materials/MaterialInterface.h"
#include "Math/Box.h"

// ---- Unified Mesh Visualization Preview Contributor ----

namespace ArcUnifiedMeshVisualizationPreviewContributor
{

/** Add a primitive component to the scene and the owner actor. */
void RegisterComponent(UPrimitiveComponent* Comp, AActor* PreviewActor, FPreviewScene& Scene)
{
	PreviewActor->AddOwnedComponent(Comp);
	Scene.AddComponent(Comp, FTransform::Identity);
}

} // namespace ArcUnifiedMeshVisualizationPreviewContributor

bool FArcUnifiedMeshVisualizationPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcUnifiedMeshVisualizationTrait* UnifiedTrait = Cast<UArcUnifiedMeshVisualizationTrait>(Trait);
	return UnifiedTrait && UnifiedTrait->IsValid();
}

void FArcUnifiedMeshVisualizationPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig)
{
	OwningConfig = InOwningConfig;
	CachedTrait = const_cast<UArcUnifiedMeshVisualizationTrait*>(CastChecked<UArcUnifiedMeshVisualizationTrait>(Trait));

	const TArray<FArcUnifiedMeshEntry>& Entries = CachedTrait->GetMeshEntries();

	// Count only valid entries — invalid entries (both/neither set) are skipped at runtime.
	int32 ValidCount = 0;
	for (const FArcUnifiedMeshEntry& Entry : Entries)
	{
		if (Entry.IsValidEntry())
		{
			++ValidCount;
		}
	}

	// Remove excess components.
	while (PreviewComponents.Num() > ValidCount)
	{
		UPrimitiveComponent* Comp = PreviewComponents.Pop();
		if (Comp)
		{
			Scene.RemoveComponent(Comp);
		}
	}

	// Rebuild — we may need to swap component type (static <-> skeletal) for an index,
	// so collect the per-valid-entry work first.
	int32 CompIdx = 0;
	for (const FArcUnifiedMeshEntry& Entry : Entries)
	{
		if (!Entry.IsValidEntry())
		{
			continue;
		}

		const bool bSkinned = Entry.IsSkinnedEntry();

		// Ensure a component exists at CompIdx.
		if (!PreviewComponents.IsValidIndex(CompIdx))
		{
			UPrimitiveComponent* NewComp = nullptr;
			if (bSkinned)
			{
				NewComp = NewObject<USkeletalMeshComponent>(PreviewActor);
			}
			else
			{
				NewComp = NewObject<UStaticMeshComponent>(PreviewActor);
			}
			ArcUnifiedMeshVisualizationPreviewContributor::RegisterComponent(NewComp, PreviewActor, Scene);
			PreviewComponents.Add(NewComp);
		}
		else
		{
			// If the type changed (e.g. user switched mesh kind), replace the component.
			UPrimitiveComponent* Existing = PreviewComponents[CompIdx];
			const bool bExistingIsSkeletal = Existing->IsA<USkeletalMeshComponent>();
			if (bSkinned != bExistingIsSkeletal)
			{
				Scene.RemoveComponent(Existing);
				UPrimitiveComponent* NewComp = nullptr;
				if (bSkinned)
				{
					NewComp = NewObject<USkeletalMeshComponent>(PreviewActor);
				}
				else
				{
					NewComp = NewObject<UStaticMeshComponent>(PreviewActor);
				}
				ArcUnifiedMeshVisualizationPreviewContributor::RegisterComponent(NewComp, PreviewActor, Scene);
				PreviewComponents[CompIdx] = NewComp;
			}
		}

		UPrimitiveComponent* Comp = PreviewComponents[CompIdx];

		if (bSkinned)
		{
			USkeletalMeshComponent* SkelComp = CastChecked<USkeletalMeshComponent>(Comp);
			USkinnedAsset* SkinnedAsset = Entry.SkinnedAsset.LoadSynchronous();
			SkelComp->SetSkinnedAssetAndUpdate(SkinnedAsset);
			SkelComp->SetVisibility(SkinnedAsset != nullptr);
		}
		else
		{
			UStaticMeshComponent* StaticComp = CastChecked<UStaticMeshComponent>(Comp);
			UStaticMesh* StaticMesh = Entry.StaticMesh.LoadSynchronous();
			StaticComp->SetStaticMesh(StaticMesh);
			StaticComp->SetVisibility(StaticMesh != nullptr);
		}

		Comp->SetRelativeTransform(Entry.RelativeTransform);

		for (int32 MatIdx = 0; MatIdx < Entry.MaterialOverrides.Num(); ++MatIdx)
		{
			UMaterialInterface* Mat = Entry.MaterialOverrides[MatIdx].LoadSynchronous();
			if (Mat)
			{
				Comp->SetMaterial(MatIdx, Mat);
			}
		}

		Comp->MarkRenderStateDirty();

		++CompIdx;
	}
}

void FArcUnifiedMeshVisualizationPreviewContributor::Clear(FPreviewScene& Scene)
{
	for (UPrimitiveComponent* Comp : PreviewComponents)
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

FBox FArcUnifiedMeshVisualizationPreviewContributor::GetBounds() const
{
	FBox CombinedBox(ForceInit);
	for (UPrimitiveComponent* Comp : PreviewComponents)
	{
		if (!Comp)
		{
			continue;
		}
		UStaticMeshComponent* StaticComp = Cast<UStaticMeshComponent>(Comp);
		USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Comp);
		if ((StaticComp && StaticComp->GetStaticMesh()) || (SkelComp && SkelComp->GetSkinnedAsset()))
		{
			CombinedBox += Comp->Bounds.GetBox();
		}
	}
	return CombinedBox;
}

FTransform FArcUnifiedMeshVisualizationPreviewContributor::GetSelectableTransform(int32 Index) const
{
	if (PreviewComponents.IsValidIndex(Index) && PreviewComponents[Index])
	{
		return PreviewComponents[Index]->GetRelativeTransform();
	}
	return FTransform::Identity;
}

void FArcUnifiedMeshVisualizationPreviewContributor::SetSelectableTransform(int32 Index, const FTransform& NewTransform)
{
	if (CachedTrait && PreviewComponents.IsValidIndex(Index) && PreviewComponents[Index])
	{
		CachedTrait->SetMeshEntryTransform(Index, NewTransform);
		CachedTrait->MarkPackageDirty();
		PreviewComponents[Index]->SetRelativeTransform(NewTransform);
		PreviewComponents[Index]->MarkRenderStateDirty();
	}
}

UObject* FArcUnifiedMeshVisualizationPreviewContributor::GetMutableTraitObject() const
{
	return CachedTrait;
}

int32 FArcUnifiedMeshVisualizationPreviewContributor::GetPreviewPrimitiveCount() const
{
	return PreviewComponents.Num();
}

UPrimitiveComponent* FArcUnifiedMeshVisualizationPreviewContributor::GetPreviewPrimitive(int32 Index) const
{
	if (PreviewComponents.IsValidIndex(Index))
	{
		return PreviewComponents[Index];
	}
	return nullptr;
}

void FArcUnifiedMeshVisualizationPreviewContributor::SetSelectionHighlight(int32 Index)
{
	if (SelectedMeshIndex != INDEX_NONE && PreviewComponents.IsValidIndex(SelectedMeshIndex) && PreviewComponents[SelectedMeshIndex])
	{
		PreviewComponents[SelectedMeshIndex]->SetRenderCustomDepth(false);
	}

	SelectedMeshIndex = Index;

	if (SelectedMeshIndex != INDEX_NONE && PreviewComponents.IsValidIndex(SelectedMeshIndex) && PreviewComponents[SelectedMeshIndex])
	{
		PreviewComponents[SelectedMeshIndex]->SetRenderCustomDepth(true);
		PreviewComponents[SelectedMeshIndex]->SetCustomDepthStencilValue(1);
	}
}
