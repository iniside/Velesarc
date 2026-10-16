// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcWorkAreaBoundsPreviewContributor.h"

#include "ArcMass/Spatial/ArcWorkAreaBoundsTrait.h"
#include "PrimitiveDrawingUtils.h"

bool FArcWorkAreaBoundsPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcWorkAreaBoundsTrait* WorkAreaTrait = Cast<UArcWorkAreaBoundsTrait>(Trait);
	return WorkAreaTrait != nullptr && WorkAreaTrait->IsValid();
}

void FArcWorkAreaBoundsPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig)
{
	OwningConfig = InOwningConfig;
	const UArcWorkAreaBoundsTrait* WorkAreaTrait = CastChecked<UArcWorkAreaBoundsTrait>(Trait);
	Extent = WorkAreaTrait->GetExtent();
	bHasWorkArea = true;
}

void FArcWorkAreaBoundsPreviewContributor::Clear(FPreviewScene& Scene)
{
	Extent = FVector::ZeroVector;
	bHasWorkArea = false;
}

void FArcWorkAreaBoundsPreviewContributor::Draw(FPrimitiveDrawInterface* PDI, int32 ContributorIndex)
{
	if (!bHasWorkArea) { return; }

	// Extent is a half-extent authored in local space.
	DrawWireBox(PDI, FBox(-Extent, Extent), FColor::Yellow, SDPG_Foreground);
}

FBox FArcWorkAreaBoundsPreviewContributor::GetBounds() const
{
	if (!bHasWorkArea) { return FBox(ForceInit); }
	return FBox(-Extent, Extent);
}
