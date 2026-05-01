// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyEditor/Contributors/ArcSettlementRadiusDrawContributor.h"
#include "EditorVisualization/ArcEditorEntityDrawComponent.h"
#include "Mass/ArcSettlementTrait.h"
#include "Mass/ArcEconomyFragments.h"

TSubclassOf<UMassEntityTraitBase> FArcSettlementRadiusDrawContributor::GetTraitClass() const
{
	return UArcSettlementTrait::StaticClass();
}

void FArcSettlementRadiusDrawContributor::CollectShapes(
	UArcEditorEntityDrawComponent& DrawComponent,
	const UMassEntityTraitBase* Trait,
	TConstArrayView<FTransform> Transforms,
	const FArcEditorDrawContext& Context)
{
	const UArcSettlementTrait* SettlementTrait = Cast<UArcSettlementTrait>(Trait);
	if (!SettlementTrait)
	{
		return;
	}

	const float Radius = SettlementTrait->SettlementConfig.SettlementRadius;
	const FColor CircleColor = FColor(80, 140, 255);

	for (const FTransform& Transform : Transforms)
	{
		const FVector Center = Transform.GetLocation();
		DrawComponent.AddCircle(
			Center,
			Radius,
			FVector::YAxisVector,
			FVector::XAxisVector,
			CircleColor,
			3.f,
			64
		);

		DrawComponent.AddText(
			Center + FVector(0, 0, 200),
			SettlementTrait->SettlementConfig.SettlementName.ToString(),
			CircleColor
		);
	}
}
