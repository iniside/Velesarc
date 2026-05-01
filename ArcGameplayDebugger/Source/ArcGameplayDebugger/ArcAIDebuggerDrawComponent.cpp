// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAIDebuggerDrawComponent.h"

void UArcAIDebuggerDrawComponent::UpdatePlanData(const FArcAIDebugPlanDrawData& InData)
{
	Data = InData;
	RefreshShapes();
}

void UArcAIDebuggerDrawComponent::CollectShapes()
{
	if (!Data.bHasEntity)
	{
		return;
	}

	const FVector EntityLoc = Data.EntityLocation + FVector(0, 0, Data.ZOffset);
	StoredSpheres.Emplace(Data.StepSphereRadius * 1.5f, EntityLoc, Data.EntityColor);
	StoredLines.Emplace(Data.PlayerLocation + FVector(0, 0, Data.ZOffset), EntityLoc, FColor(100, 200, 255), 2.f);

	if (!Data.Steps.IsEmpty())
	{
		const FVector FirstStepLoc = Data.Steps[0].Location;
		StoredLines.Emplace(EntityLoc, FirstStepLoc, FColor(200, 200, 200), 3.f);

		for (int32 i = 0; i < Data.Steps.Num(); ++i)
		{
			const FArcAIDebugPlanStep& Step = Data.Steps[i];
			StoredSpheres.Emplace(Data.StepSphereRadius, Step.Location, Step.Color);
			StoredTexts.Emplace(Step.Label, Step.Location + FVector(0, 0, 40.f), FColor::White);
			if (i + 1 < Data.Steps.Num())
			{
				StoredArrows.Emplace(Step.Location, Data.Steps[i + 1].Location, FColor(200, 200, 200));
			}
		}
	}
}
