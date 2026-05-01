// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPlanFeasibilityDebuggerDrawComponent.h"

void UArcPlanFeasibilityDebuggerDrawComponent::UpdatePlanData(const FArcPlanFeasibilityDebugDrawData& InData)
{
	Data = InData;
	RefreshShapes();
}

void UArcPlanFeasibilityDebuggerDrawComponent::CollectShapes()
{
	const FVector Origin = Data.SearchOrigin;

	// Search radius circle
	StoredCircles.Emplace(Origin, Data.SearchRadius, 64, FVector::RightVector, FVector::ForwardVector, FColor(100, 200, 255, 128), 2.f);
	StoredSpheres.Emplace(20.f, Origin + FVector(0, 0, 50.f), FColor::Cyan);

	if (!Data.bHasSelectedPlan || Data.Steps.IsEmpty())
	{
		return;
	}

	StoredArrows.Emplace(Origin, Data.Steps[0].Location, FColor::Cyan);

	for (int32 i = 0; i < Data.Steps.Num(); ++i)
	{
		const FArcPlanFeasibilityDebugStep& Step = Data.Steps[i];
		float SphereRadius = (i == 0) ? 25.f : 20.f;
		StoredSpheres.Emplace(SphereRadius, Step.Location, Step.Color);
		StoredTexts.Emplace(Step.Label, Step.Location + FVector(0, 0, 50.f), FColor::White);
		if (i + 1 < Data.Steps.Num())
		{
			FColor ArrowColor = (i == 0) ? FColor::Cyan : FColor(200, 200, 200);
			StoredArrows.Emplace(Step.Location, Data.Steps[i + 1].Location, ArrowColor);
		}
	}
}
