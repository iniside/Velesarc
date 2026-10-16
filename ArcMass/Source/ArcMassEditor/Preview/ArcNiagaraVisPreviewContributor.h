// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Preview/ArcMassPreviewContributorInterface.h"

class UArcNiagaraVisTrait;
class UNiagaraComponent;

class FArcNiagaraVisPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig) override;
	virtual void Clear(FPreviewScene& Scene) override;
	virtual FBox GetBounds() const override;

	virtual FTransform GetSelectableTransform(int32 Index) const override;
	virtual void SetSelectableTransform(int32 Index, const FTransform& NewTransform) override;
	virtual UObject* GetMutableTraitObject() const override;
	virtual int32 GetPreviewPrimitiveCount() const override { return PreviewComponent ? 1 : 0; }
	virtual UPrimitiveComponent* GetPreviewPrimitive(int32 Index) const override;

private:
	TObjectPtr<UNiagaraComponent> PreviewComponent = nullptr;
	TObjectPtr<UArcNiagaraVisTrait> CachedTrait = nullptr;
};
