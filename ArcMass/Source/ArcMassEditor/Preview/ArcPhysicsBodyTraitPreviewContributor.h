// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Preview/ArcMassPreviewContributorInterface.h"
#include "PhysicsEngine/AggregateGeom.h"

class UArcMassPhysicsBodyTrait;

class FArcPhysicsBodyTraitPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig) override;
	virtual void Clear(FPreviewScene& Scene) override;
	virtual void Draw(FPrimitiveDrawInterface* PDI, int32 ContributorIndex) override;
	virtual FBox GetBounds() const override;

	virtual FTransform GetSelectableTransform(int32 Index) const override;
	virtual void SetSelectableTransform(int32 Index, const FTransform& NewWorldTransform) override;
	virtual UObject* GetMutableTraitObject() const override;
	virtual void SetSelectionHighlight(int32 Index) override { SelectedShapeIndex = Index; }

private:
	FKAggregateGeom CollisionGeometry;
	bool bHasCollision = false;
	TObjectPtr<UArcMassPhysicsBodyTrait> CachedTrait = nullptr;
	int32 SelectedShapeIndex = INDEX_NONE;
};
