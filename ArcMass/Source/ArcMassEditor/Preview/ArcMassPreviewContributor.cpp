// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcMassPreviewContributor.h"

#include "AdvancedPreviewScene.h"
#include "GameFramework/Actor.h"
#include "ArcMass/Visualization/ArcMassEntityVisualizationTrait.h"
#include "ArcMass/Visualization/ArcMeshEntityVisualizationTrait.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Math/Box.h"
#include "SceneManagement.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "ArcMass/Visualization/ArcSkinnedMeshEntityVisualizationTrait.h"
#include "ArcMass/Visualization/ArcFastGeoVisualizationTrait.h"
#include "Components/SkeletalMeshComponent.h"

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

// ---- Visualization Contributor ----

bool FArcVisualizationPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcEntityVisualizationTrait* VisTrait = Cast<UArcEntityVisualizationTrait>(Trait);
	return VisTrait && VisTrait->IsExtractionValid() && VisTrait->GetExtractedMesh() != nullptr;
}

void FArcVisualizationPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor)
{
	const UArcEntityVisualizationTrait* VisTrait = CastChecked<UArcEntityVisualizationTrait>(Trait);

	if (!PreviewComponent)
	{
		PreviewComponent = NewObject<UStaticMeshComponent>(PreviewActor);
		PreviewActor->AddOwnedComponent(PreviewComponent);
		Scene.AddComponent(PreviewComponent, VisTrait->GetExtractedComponentTransform());
	}

	PreviewComponent->SetStaticMesh(VisTrait->GetExtractedMesh());
	PreviewComponent->SetRelativeTransform(VisTrait->GetExtractedComponentTransform());

	const TArray<TObjectPtr<UMaterialInterface>>& Materials = VisTrait->GetExtractedMaterials();
	for (int32 Idx = 0; Idx < Materials.Num(); ++Idx)
	{
		PreviewComponent->SetMaterial(Idx, Materials[Idx]);
	}

	PreviewComponent->MarkRenderStateDirty();
}

void FArcVisualizationPreviewContributor::Clear(FAdvancedPreviewScene& Scene)
{
	if (PreviewComponent)
	{
		Scene.RemoveComponent(PreviewComponent);
		PreviewComponent = nullptr;
	}
}

FBox FArcVisualizationPreviewContributor::GetBounds() const
{
	if (PreviewComponent && PreviewComponent->GetStaticMesh())
	{
		return PreviewComponent->Bounds.GetBox();
	}
	return FBox(ForceInit);
}

// ---- Physics Collision Contributor ----

bool FArcPhysicsCollisionPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcEntityVisualizationTrait* VisTrait = Cast<UArcEntityVisualizationTrait>(Trait);
	if (!VisTrait || !VisTrait->IsExtractionValid())
	{
		return false;
	}

	// Use extracted body setup if available, otherwise fall back to mesh's own body setup
	if (VisTrait->GetExtractedBodySetup())
	{
		return true;
	}

	UStaticMesh* Mesh = VisTrait->GetExtractedMesh();
	return Mesh && Mesh->GetBodySetup() != nullptr;
}

void FArcPhysicsCollisionPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor)
{
	const UArcEntityVisualizationTrait* VisTrait = CastChecked<UArcEntityVisualizationTrait>(Trait);
	UBodySetup* BodySetup = VisTrait->GetExtractedBodySetup();
	if (BodySetup)
	{
		CollisionGeometry = BodySetup->AggGeom;
		CollisionTransform = FTransform::Identity;
		bHasCollision = true;
	}
	else
	{
		UStaticMesh* Mesh = VisTrait->GetExtractedMesh();
		if (Mesh && Mesh->GetBodySetup())
		{
			CollisionGeometry = Mesh->GetBodySetup()->AggGeom;
			CollisionTransform = VisTrait->GetExtractedComponentTransform();
			bHasCollision = true;
		}
	}
}

void FArcPhysicsCollisionPreviewContributor::Clear(FAdvancedPreviewScene& Scene)
{
	CollisionGeometry = FKAggregateGeom();
	CollisionTransform = FTransform::Identity;
	bHasCollision = false;
}

void FArcPhysicsCollisionPreviewContributor::Draw(FPrimitiveDrawInterface* PDI, int32 ContributorIndex)
{
	if (!bHasCollision) { return; }
	const FColor CollisionColor(0, 255, 0);
	const FVector ScaleOne(1.0);
	int32 GlobalIndex = 0;
	for (int32 Idx = 0; Idx < CollisionGeometry.SphereElems.Num(); ++Idx, ++GlobalIndex)
	{
		const FKSphereElem& Elem = CollisionGeometry.SphereElems[Idx];
		PDI->SetHitProxy(new HArcMassPreviewHitProxy(ContributorIndex, GlobalIndex));
		Elem.DrawElemWire(PDI, Elem.GetTransform() * CollisionTransform, ScaleOne, CollisionColor);
		PDI->SetHitProxy(nullptr);
	}
	for (int32 Idx = 0; Idx < CollisionGeometry.BoxElems.Num(); ++Idx, ++GlobalIndex)
	{
		const FKBoxElem& Elem = CollisionGeometry.BoxElems[Idx];
		PDI->SetHitProxy(new HArcMassPreviewHitProxy(ContributorIndex, GlobalIndex));
		Elem.DrawElemWire(PDI, Elem.GetTransform() * CollisionTransform, ScaleOne, CollisionColor);
		PDI->SetHitProxy(nullptr);
	}
	for (int32 Idx = 0; Idx < CollisionGeometry.SphylElems.Num(); ++Idx, ++GlobalIndex)
	{
		const FKSphylElem& Elem = CollisionGeometry.SphylElems[Idx];
		PDI->SetHitProxy(new HArcMassPreviewHitProxy(ContributorIndex, GlobalIndex));
		Elem.DrawElemWire(PDI, Elem.GetTransform() * CollisionTransform, ScaleOne, CollisionColor);
		PDI->SetHitProxy(nullptr);
	}
	for (int32 Idx = 0; Idx < CollisionGeometry.ConvexElems.Num(); ++Idx, ++GlobalIndex)
	{
		const FKConvexElem& Elem = CollisionGeometry.ConvexElems[Idx];
		PDI->SetHitProxy(new HArcMassPreviewHitProxy(ContributorIndex, GlobalIndex));
		Elem.DrawElemWire(PDI, Elem.GetTransform() * CollisionTransform, 1.0f, CollisionColor);
		PDI->SetHitProxy(nullptr);
	}
}

FBox FArcPhysicsCollisionPreviewContributor::GetBounds() const
{
	if (!bHasCollision) { return FBox(ForceInit); }
	return CollisionGeometry.CalcAABB(CollisionTransform);
}

// ---- Mesh Visualization Contributor (direct mesh) ----

bool FArcMeshVisualizationPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcMeshEntityVisualizationTrait* MeshTrait = Cast<UArcMeshEntityVisualizationTrait>(Trait);
	return MeshTrait && MeshTrait->IsValid();
}

void FArcMeshVisualizationPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor)
{
	CachedTrait = const_cast<UArcMeshEntityVisualizationTrait*>(CastChecked<UArcMeshEntityVisualizationTrait>(Trait));

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

void FArcMeshVisualizationPreviewContributor::Clear(FAdvancedPreviewScene& Scene)
{
	if (PreviewComponent)
	{
		Scene.RemoveComponent(PreviewComponent);
		PreviewComponent = nullptr;
	}
	CachedTrait = nullptr;
}

FBox FArcMeshVisualizationPreviewContributor::GetBounds() const
{
	if (PreviewComponent && PreviewComponent->GetStaticMesh())
	{
		return PreviewComponent->Bounds.GetBox();
	}
	return FBox(ForceInit);
}

int32 FArcMeshVisualizationPreviewContributor::GetSelectableCount() const
{
	return (PreviewComponent && PreviewComponent->GetStaticMesh()) ? 1 : 0;
}

FTransform FArcMeshVisualizationPreviewContributor::GetSelectableTransform(int32 Index) const
{
	if (Index == 0 && PreviewComponent)
	{
		return PreviewComponent->GetRelativeTransform();
	}
	return FTransform::Identity;
}

void FArcMeshVisualizationPreviewContributor::SetSelectableTransform(int32 Index, const FTransform& NewTransform)
{
	if (Index == 0 && CachedTrait && PreviewComponent)
	{
		CachedTrait->SetComponentTransform(NewTransform);
		CachedTrait->MarkPackageDirty();
		PreviewComponent->SetRelativeTransform(NewTransform);
		PreviewComponent->MarkRenderStateDirty();
	}
}

UObject* FArcMeshVisualizationPreviewContributor::GetMutableTraitObject() const
{
	return CachedTrait;
}

// ---- FastGeo Visualization Contributor ----

bool FArcFastGeoVisualizationPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcFastGeoVisualizationTrait* FastGeoTrait = Cast<UArcFastGeoVisualizationTrait>(Trait);
	return FastGeoTrait && FastGeoTrait->IsValid();
}

void FArcFastGeoVisualizationPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor)
{
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

void FArcFastGeoVisualizationPreviewContributor::Clear(FAdvancedPreviewScene& Scene)
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

int32 FArcFastGeoVisualizationPreviewContributor::GetSelectableCount() const
{
	return (PreviewComponent && PreviewComponent->GetStaticMesh()) ? 1 : 0;
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

// ---- Skinned Mesh Visualization Contributor ----

bool FArcSkinnedMeshVisualizationPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcSkinnedMeshEntityVisualizationTrait* SkinnedTrait = Cast<UArcSkinnedMeshEntityVisualizationTrait>(Trait);
	return SkinnedTrait && SkinnedTrait->IsValid();
}

void FArcSkinnedMeshVisualizationPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor)
{
	CachedTrait = const_cast<UArcSkinnedMeshEntityVisualizationTrait*>(CastChecked<UArcSkinnedMeshEntityVisualizationTrait>(Trait));

	if (!PreviewComponent)
	{
		PreviewComponent = NewObject<USkeletalMeshComponent>(PreviewActor);
		PreviewActor->AddOwnedComponent(PreviewComponent);
		Scene.AddComponent(PreviewComponent, CachedTrait->GetComponentTransform());
	}

	PreviewComponent->SetSkinnedAssetAndUpdate(CachedTrait->GetSkinnedAsset());
	PreviewComponent->SetRelativeTransform(CachedTrait->GetComponentTransform());

	const TArray<TObjectPtr<UMaterialInterface>>& Materials = CachedTrait->GetMaterialOverrides();
	for (int32 Idx = 0; Idx < Materials.Num(); ++Idx)
	{
		PreviewComponent->SetMaterial(Idx, Materials[Idx]);
	}

	PreviewComponent->MarkRenderStateDirty();
}

void FArcSkinnedMeshVisualizationPreviewContributor::Clear(FAdvancedPreviewScene& Scene)
{
	if (PreviewComponent)
	{
		Scene.RemoveComponent(PreviewComponent);
		PreviewComponent = nullptr;
	}
	CachedTrait = nullptr;
}

FBox FArcSkinnedMeshVisualizationPreviewContributor::GetBounds() const
{
	if (PreviewComponent && PreviewComponent->GetSkinnedAsset())
	{
		return PreviewComponent->Bounds.GetBox();
	}
	return FBox(ForceInit);
}

int32 FArcSkinnedMeshVisualizationPreviewContributor::GetSelectableCount() const
{
	return (PreviewComponent && PreviewComponent->GetSkinnedAsset()) ? 1 : 0;
}

FTransform FArcSkinnedMeshVisualizationPreviewContributor::GetSelectableTransform(int32 Index) const
{
	if (Index == 0 && PreviewComponent)
	{
		return PreviewComponent->GetRelativeTransform();
	}
	return FTransform::Identity;
}

void FArcSkinnedMeshVisualizationPreviewContributor::SetSelectableTransform(int32 Index, const FTransform& NewTransform)
{
	if (Index == 0 && CachedTrait && PreviewComponent)
	{
		CachedTrait->SetComponentTransform(NewTransform);
		CachedTrait->MarkPackageDirty();
		PreviewComponent->SetRelativeTransform(NewTransform);
		PreviewComponent->MarkRenderStateDirty();
	}
}

UObject* FArcSkinnedMeshVisualizationPreviewContributor::GetMutableTraitObject() const
{
	return CachedTrait;
}
