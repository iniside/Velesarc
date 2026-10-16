// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Preview/ArcMassPreviewContributorInterface.h"

class UArcUnifiedMeshVisualizationTrait;
class UStaticMeshComponent;
class USkeletalMeshComponent;

class FArcUnifiedMeshVisualizationPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig) override;
	virtual void Clear(FPreviewScene& Scene) override;
	virtual FBox GetBounds() const override;

	virtual FTransform GetSelectableTransform(int32 Index) const override;
	virtual void SetSelectableTransform(int32 Index, const FTransform& NewTransform) override;
	virtual UObject* GetMutableTraitObject() const override;

	virtual int32 GetPreviewPrimitiveCount() const override;
	virtual UPrimitiveComponent* GetPreviewPrimitive(int32 Index) const override;

	virtual void SetSelectionHighlight(int32 Index) override;

private:
	/** One element per valid mesh entry; either UStaticMeshComponent or USkeletalMeshComponent. */
	TArray<TObjectPtr<UPrimitiveComponent>> PreviewComponents;
	TObjectPtr<UArcUnifiedMeshVisualizationTrait> CachedTrait = nullptr;
	int32 SelectedMeshIndex = INDEX_NONE;
};
