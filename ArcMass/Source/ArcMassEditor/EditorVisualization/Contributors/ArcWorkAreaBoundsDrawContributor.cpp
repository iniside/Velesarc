// Copyright Lukasz Baran. All Rights Reserved.

#include "EditorVisualization/Contributors/ArcWorkAreaBoundsDrawContributor.h"
#include "EditorVisualization/ArcEditorDrawCollector.h"
#include "ArcMass/Spatial/ArcWorkAreaBoundsTrait.h"

TSubclassOf<UMassEntityTraitBase> FArcWorkAreaBoundsDrawContributor::GetTraitClass() const
{
	return UArcWorkAreaBoundsTrait::StaticClass();
}

void FArcWorkAreaBoundsDrawContributor::CollectShapes(
	FArcEditorDrawCollector& Collector,
	const UMassEntityTraitBase* Trait,
	TConstArrayView<FTransform> Transforms,
	const FArcEditorDrawContext& Context)
{
	const UArcWorkAreaBoundsTrait* WorkAreaTrait = Cast<UArcWorkAreaBoundsTrait>(Trait);
	if (!WorkAreaTrait || !WorkAreaTrait->IsValid())
	{
		return;
	}

	const FColor Color = FColor::Yellow;

	for (const FTransform& Transform : Transforms)
	{
		// Extent is a half-extent authored in local space; scale carries the placed size.
		const FVector ScaledExtent = WorkAreaTrait->GetExtent() * Transform.GetScale3D();
		Collector.AddBox(Transform.GetLocation(), ScaledExtent, Transform.GetRotation(), Color, 2.f);
	}
}
