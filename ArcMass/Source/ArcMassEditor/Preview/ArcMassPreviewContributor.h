// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "HitProxies.h"

class AActor;
class FAdvancedPreviewScene;
class FPrimitiveDrawInterface;
class UArcMeshEntityVisualizationTrait;
class UMassEntityTraitBase;
class UArcSkinnedMeshEntityVisualizationTrait;

struct HArcMassPreviewHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	int32 ContributorIndex;
	int32 ElementIndex;

	HArcMassPreviewHitProxy(int32 InContributorIndex, int32 InElementIndex)
		: HHitProxy(HPP_Foreground)
		, ContributorIndex(InContributorIndex)
		, ElementIndex(InElementIndex)
	{}
};

class IArcMassPreviewContributor
{
public:
	virtual ~IArcMassPreviewContributor() = default;
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const = 0;
	virtual void Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor) = 0;
	virtual void Clear(FAdvancedPreviewScene& Scene) = 0;
	virtual void Draw(FPrimitiveDrawInterface* PDI, int32 ContributorIndex) {}
	virtual FBox GetBounds() const { return FBox(ForceInit); }
	virtual int32 GetSelectableCount() const { return 0; }
	virtual FTransform GetSelectableTransform(int32 Index) const { return FTransform::Identity; }
	virtual void SetSelectableTransform(int32 Index, const FTransform& NewTransform) {}
	virtual UObject* GetMutableTraitObject() const { return nullptr; }
	virtual void SetSelectionHighlight(int32 Index) {}
	virtual int32 GetPreviewPrimitiveCount() const { return 0; }
	virtual UPrimitiveComponent* GetPreviewPrimitive(int32 Index) const { return nullptr; }
};

class FArcVisualizationPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor) override;
	virtual void Clear(FAdvancedPreviewScene& Scene) override;
	virtual FBox GetBounds() const override;
	virtual int32 GetPreviewPrimitiveCount() const override { return PreviewComponent ? 1 : 0; }
	virtual UPrimitiveComponent* GetPreviewPrimitive(int32 Index) const override { return (Index == 0) ? PreviewComponent.Get() : nullptr; }

	UStaticMeshComponent* GetPreviewComponent() const { return PreviewComponent; }

private:
	TObjectPtr<UStaticMeshComponent> PreviewComponent = nullptr;
};

class FArcPhysicsCollisionPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor) override;
	virtual void Clear(FAdvancedPreviewScene& Scene) override;
	virtual void Draw(FPrimitiveDrawInterface* PDI, int32 ContributorIndex) override;
	virtual FBox GetBounds() const override;

private:
	FKAggregateGeom CollisionGeometry;
	FTransform CollisionTransform = FTransform::Identity;
	bool bHasCollision = false;
};

class FArcMeshVisualizationPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor) override;
	virtual void Clear(FAdvancedPreviewScene& Scene) override;
	virtual FBox GetBounds() const override;

	virtual int32 GetSelectableCount() const override;
	virtual FTransform GetSelectableTransform(int32 Index) const override;
	virtual void SetSelectableTransform(int32 Index, const FTransform& NewTransform) override;
	virtual UObject* GetMutableTraitObject() const override;
	virtual int32 GetPreviewPrimitiveCount() const override { return PreviewComponent ? 1 : 0; }
	virtual UPrimitiveComponent* GetPreviewPrimitive(int32 Index) const override { return (Index == 0) ? PreviewComponent.Get() : nullptr; }

	UStaticMeshComponent* GetPreviewComponent() const { return PreviewComponent; }

private:
	TObjectPtr<UStaticMeshComponent> PreviewComponent = nullptr;
	TObjectPtr<UArcMeshEntityVisualizationTrait> CachedTrait = nullptr;
};

class UArcFastGeoVisualizationTrait;

class FArcFastGeoVisualizationPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor) override;
	virtual void Clear(FAdvancedPreviewScene& Scene) override;
	virtual FBox GetBounds() const override;

	virtual int32 GetSelectableCount() const override;
	virtual FTransform GetSelectableTransform(int32 Index) const override;
	virtual void SetSelectableTransform(int32 Index, const FTransform& NewTransform) override;
	virtual UObject* GetMutableTraitObject() const override;
	virtual int32 GetPreviewPrimitiveCount() const override { return PreviewComponent ? 1 : 0; }
	virtual UPrimitiveComponent* GetPreviewPrimitive(int32 Index) const override { return (Index == 0) ? PreviewComponent.Get() : nullptr; }

private:
	TObjectPtr<UStaticMeshComponent> PreviewComponent = nullptr;
	TObjectPtr<UArcFastGeoVisualizationTrait> CachedTrait = nullptr;
};

class FArcSkinnedMeshVisualizationPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor) override;
	virtual void Clear(FAdvancedPreviewScene& Scene) override;
	virtual FBox GetBounds() const override;

	virtual int32 GetSelectableCount() const override;
	virtual FTransform GetSelectableTransform(int32 Index) const override;
	virtual void SetSelectableTransform(int32 Index, const FTransform& NewTransform) override;
	virtual UObject* GetMutableTraitObject() const override;
	virtual int32 GetPreviewPrimitiveCount() const override { return PreviewComponent ? 1 : 0; }
	virtual UPrimitiveComponent* GetPreviewPrimitive(int32 Index) const override { return (Index == 0) ? PreviewComponent.Get() : nullptr; }

	USkeletalMeshComponent* GetPreviewComponent() const { return PreviewComponent; }

private:
	TObjectPtr<USkeletalMeshComponent> PreviewComponent = nullptr;
	TObjectPtr<UArcSkinnedMeshEntityVisualizationTrait> CachedTrait = nullptr;
};

