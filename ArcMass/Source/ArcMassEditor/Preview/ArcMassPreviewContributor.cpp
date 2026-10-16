// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcMassPreviewContributor.h"

#include "AdvancedPreviewScene.h"
#include "MassEntityConfigAsset.h"
#include "MassLogging.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Math/Box.h"
#include "SceneManagement.h"
#include "ArcMass/Visualization/ArcFastGeoVisualizationTrait.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/ConvexElem.h"

namespace ArcMassPreviewContributor
{

void SetShapeElemTransform(FKShapeElem* Elem, const FTransform& InTransform)
{
	switch (Elem->GetShapeType())
	{
	case EAggCollisionShape::Sphere:
		static_cast<FKSphereElem*>(Elem)->SetTransform(InTransform);
		break;
	case EAggCollisionShape::Box:
		static_cast<FKBoxElem*>(Elem)->SetTransform(InTransform);
		break;
	case EAggCollisionShape::Sphyl:
		static_cast<FKSphylElem*>(Elem)->SetTransform(InTransform);
		break;
	case EAggCollisionShape::Convex:
		static_cast<FKConvexElem*>(Elem)->SetTransform(InTransform);
		break;
	default:
		break;
	}
}

} // namespace ArcMassPreviewContributor

IMPLEMENT_HIT_PROXY(HArcMassPreviewHitProxy, HHitProxy);

void ArcMassPreview::ApplyConfigToContributors(
	const UMassEntityConfigAsset& Config,
	TArray<TUniquePtr<IArcMassPreviewContributor>>& Contributors,
	FPreviewScene& Scene,
	AActor* PreviewActor)
{
	TArray<bool> ContributorClaimed;
	ContributorClaimed.Init(false, Contributors.Num());

	TArray<FMassEditableTraitInstance> Traits = Config.GetConfig().GetCombinedEditableTraits();
	for (const FMassEditableTraitInstance& TraitInstance : Traits)
	{
		if (TraitInstance.Trait == nullptr)
		{
			continue;
		}

		for (int32 ContribIdx = 0; ContribIdx < Contributors.Num(); ++ContribIdx)
		{
			TUniquePtr<IArcMassPreviewContributor>& Contributor = Contributors[ContribIdx];
			if (!Contributor->CanContribute(TraitInstance.Trait))
			{
				continue;
			}

			if (ContributorClaimed[ContribIdx])
			{
				UE_LOGF(LogMass, Warning,"ArcMass preview '%ls': trait '%ls' skipped, an earlier trait of the same class already claimed this contributor.",
					*Config.GetName(), *GetNameSafe(TraitInstance.Trait));
				continue;
			}

			ContributorClaimed[ContribIdx] = true;
			Contributor->Apply(TraitInstance.Trait, Scene, PreviewActor, TraitInstance.OwningConfigAsset);
		}
	}
}

// ---- FastGeo Visualization Contributor ----

bool FArcFastGeoVisualizationPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcFastGeoVisualizationTrait* FastGeoTrait = Cast<UArcFastGeoVisualizationTrait>(Trait);
	return FastGeoTrait && FastGeoTrait->IsValid();
}

void FArcFastGeoVisualizationPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig)
{
	OwningConfig = InOwningConfig;
	CachedTrait = const_cast<UArcFastGeoVisualizationTrait*>(CastChecked<UArcFastGeoVisualizationTrait>(Trait));

	if (!PreviewComponent)
	{
		PreviewComponent = NewObject<UStaticMeshComponent>(PreviewActor);
		PreviewActor->AddOwnedComponent(PreviewComponent);
		Scene.AddComponent(PreviewComponent, CachedTrait->GetComponentTransform());
	}

	PreviewComponent->SetStaticMesh(CachedTrait->GetMesh());
	PreviewComponent->SetRelativeTransform(CachedTrait->GetComponentTransform());

	const TArray<TObjectPtr<UMaterialInterface>>& Materials = CachedTrait->GetMaterialOverrides();
	for (int32 Idx = 0; Idx < Materials.Num(); ++Idx)
	{
		PreviewComponent->SetMaterial(Idx, Materials[Idx]);
	}

	PreviewComponent->MarkRenderStateDirty();
}

void FArcFastGeoVisualizationPreviewContributor::Clear(FPreviewScene& Scene)
{
	if (PreviewComponent)
	{
		Scene.RemoveComponent(PreviewComponent);
		PreviewComponent = nullptr;
	}
	CachedTrait = nullptr;
}

FBox FArcFastGeoVisualizationPreviewContributor::GetBounds() const
{
	if (PreviewComponent && PreviewComponent->GetStaticMesh())
	{
		return PreviewComponent->Bounds.GetBox();
	}
	return FBox(ForceInit);
}

FTransform FArcFastGeoVisualizationPreviewContributor::GetSelectableTransform(int32 Index) const
{
	if (Index == 0 && PreviewComponent)
	{
		return PreviewComponent->GetRelativeTransform();
	}
	return FTransform::Identity;
}

void FArcFastGeoVisualizationPreviewContributor::SetSelectableTransform(int32 Index, const FTransform& NewTransform)
{
	if (Index == 0 && CachedTrait && PreviewComponent)
	{
		CachedTrait->SetComponentTransform(NewTransform);
		CachedTrait->MarkPackageDirty();
		PreviewComponent->SetRelativeTransform(NewTransform);
		PreviewComponent->MarkRenderStateDirty();
	}
}

UObject* FArcFastGeoVisualizationPreviewContributor::GetMutableTraitObject() const
{
	return CachedTrait;
}
