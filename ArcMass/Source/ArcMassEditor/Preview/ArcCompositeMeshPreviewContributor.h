// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMassPreviewContributor.h"

class UArcCompositeMeshVisualizationTrait;
class UStaticMeshComponent;

class FArcCompositeMeshPreviewContributor : public IArcMassPreviewContributor
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

	virtual int32 GetPreviewPrimitiveCount() const override;
	virtual UPrimitiveComponent* GetPreviewPrimitive(int32 Index) const override;

	virtual void SetSelectionHighlight(int32 Index) override;

private:
	TArray<TObjectPtr<UStaticMeshComponent>> PreviewComponents;
	TObjectPtr<UArcCompositeMeshVisualizationTrait> CachedTrait = nullptr;
	int32 SelectedMeshIndex = INDEX_NONE;
};

