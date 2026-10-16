// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Preview/ArcMassPreviewContributorInterface.h"

class FArcWorkAreaBoundsPreviewContributor : public IArcMassPreviewContributor
{
public:
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const override;
	virtual void Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig) override;
	virtual void Clear(FPreviewScene& Scene) override;
	virtual void Draw(FPrimitiveDrawInterface* PDI, int32 ContributorIndex) override;
	virtual FBox GetBounds() const override;

private:
	bool bHasWorkArea = false;
	FVector Extent = FVector::ZeroVector;
};
