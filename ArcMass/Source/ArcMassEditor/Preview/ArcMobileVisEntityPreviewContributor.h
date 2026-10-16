// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Preview/ArcMassPreviewContributorInterface.h"

class AActor;
class UPrimitiveComponent;

class FArcMobileVisEntityPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig) override;
	virtual void Clear(FPreviewScene& Scene) override;
	virtual FBox GetBounds() const override;

private:
	TWeakObjectPtr<AActor> PreviewRepresentationActor;
	TObjectPtr<UPrimitiveComponent> PreviewComponent = nullptr;
};
