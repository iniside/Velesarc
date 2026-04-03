// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcPhysicsBodyTraitPreviewContributor.h"

#include "AdvancedPreviewScene.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyTrait.h"
#include "SceneManagement.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/ConvexElem.h"

namespace ArcMassPreviewContributor
{
	void SetShapeElemTransform(FKShapeElem* Elem, const FTransform& InTransform);
}

bool FArcPhysicsBodyTraitPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcMassPhysicsBodyTrait* PhysTrait = Cast<UArcMassPhysicsBodyTrait>(Trait);
	return PhysTrait && PhysTrait->HasCollisionGeometry();
}

void FArcPhysicsBodyTraitPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor)
{
	CachedTrait = const_cast<UArcMassPhysicsBodyTrait*>(CastChecked<UArcMassPhysicsBodyTrait>(Trait));
	CollisionGeometry = CachedTrait->GetCollisionGeometry();
	bHasCollision = true;
}

void FArcPhysicsBodyTraitPreviewContributor::Clear(FAdvancedPreviewScene& Scene)
{
	CollisionGeometry = FKAggregateGeom();
	bHasCollision = false;
	CachedTrait = nullptr;
	SelectedShapeIndex = INDEX_NONE;
}

void FArcPhysicsBodyTraitPreviewContributor::Draw(FPrimitiveDrawInterface* PDI, int32 ContributorIndex)
{
	if (!bHasCollision) { return; }

	const FColor NormalColor(0, 255, 0);
	const FColor SelectedColor(255, 200, 0);
	const FVector ScaleOne(1.0);
	int32 GlobalIndex = 0;

	for (int32 Idx = 0; Idx < CollisionGeometry.SphereElems.Num(); ++Idx, ++GlobalIndex)
	{
		const FKSphereElem& Elem = CollisionGeometry.SphereElems[Idx];
		FColor Color = (GlobalIndex == SelectedShapeIndex) ? SelectedColor : NormalColor;
		PDI->SetHitProxy(new HArcMassPreviewHitProxy(ContributorIndex, GlobalIndex));
		Elem.DrawElemWire(PDI, Elem.GetTransform(), ScaleOne, Color);
		PDI->SetHitProxy(nullptr);
	}
	for (int32 Idx = 0; Idx < CollisionGeometry.BoxElems.Num(); ++Idx, ++GlobalIndex)
	{
		const FKBoxElem& Elem = CollisionGeometry.BoxElems[Idx];
		FColor Color = (GlobalIndex == SelectedShapeIndex) ? SelectedColor : NormalColor;
		PDI->SetHitProxy(new HArcMassPreviewHitProxy(ContributorIndex, GlobalIndex));
		Elem.DrawElemWire(PDI, Elem.GetTransform(), ScaleOne, Color);
		PDI->SetHitProxy(nullptr);
	}
	for (int32 Idx = 0; Idx < CollisionGeometry.SphylElems.Num(); ++Idx, ++GlobalIndex)
	{
		const FKSphylElem& Elem = CollisionGeometry.SphylElems[Idx];
		FColor Color = (GlobalIndex == SelectedShapeIndex) ? SelectedColor : NormalColor;
		PDI->SetHitProxy(new HArcMassPreviewHitProxy(ContributorIndex, GlobalIndex));
		Elem.DrawElemWire(PDI, Elem.GetTransform(), ScaleOne, Color);
		PDI->SetHitProxy(nullptr);
	}
	for (int32 Idx = 0; Idx < CollisionGeometry.ConvexElems.Num(); ++Idx, ++GlobalIndex)
	{
		const FKConvexElem& Elem = CollisionGeometry.ConvexElems[Idx];
		FColor Color = (GlobalIndex == SelectedShapeIndex) ? SelectedColor : NormalColor;
		PDI->SetHitProxy(new HArcMassPreviewHitProxy(ContributorIndex, GlobalIndex));
		Elem.DrawElemWire(PDI, Elem.GetTransform(), 1.0f, Color);
		PDI->SetHitProxy(nullptr);
	}
}

FBox FArcPhysicsBodyTraitPreviewContributor::GetBounds() const
{
	if (!bHasCollision) { return FBox(ForceInit); }
	return CollisionGeometry.CalcAABB(FTransform::Identity);
}

int32 FArcPhysicsBodyTraitPreviewContributor::GetSelectableCount() const
{
	if (!bHasCollision) { return 0; }
	return CollisionGeometry.GetElementCount();
}

FTransform FArcPhysicsBodyTraitPreviewContributor::GetSelectableTransform(int32 Index) const
{
	if (!bHasCollision) { return FTransform::Identity; }
	const FKShapeElem* Elem = CollisionGeometry.GetElement(Index);
	if (!Elem) { return FTransform::Identity; }
	return Elem->GetTransform();
}

void FArcPhysicsBodyTraitPreviewContributor::SetSelectableTransform(int32 Index, const FTransform& NewWorldTransform)
{
	if (!bHasCollision || !CachedTrait) { return; }

	FArcCollisionGeometry& MutableGeom = CachedTrait->GetMutableCollisionGeometry();
	FKShapeElem* Elem = MutableGeom.AggGeom.GetElement(Index);
	if (Elem)
	{
		ArcMassPreviewContributor::SetShapeElemTransform(Elem, NewWorldTransform);
		CachedTrait->MarkPackageDirty();
		FKShapeElem* CachedElem = CollisionGeometry.GetElement(Index);
		if (CachedElem)
		{
			ArcMassPreviewContributor::SetShapeElemTransform(CachedElem, NewWorldTransform);
		}
	}
}

UObject* FArcPhysicsBodyTraitPreviewContributor::GetMutableTraitObject() const
{
	return CachedTrait;
}
