// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPerceptionDebuggerDrawComponent.h"

void UArcPerceptionDebuggerDrawComponent::UpdatePerceptionData(const FArcPerceptionDebugDrawData& InData)
{
	Data = InData;
	RefreshShapes();
}

void UArcPerceptionDebuggerDrawComponent::CollectShapes()
{
	// Entity marker
	const FVector EntityLoc = Data.EntityMarker.Location;
	StoredSpheres.Emplace(40.f, EntityLoc, Data.EntityMarker.Color);
	StoredTexts.Emplace(Data.EntityMarker.Label, EntityLoc + FVector(0, 0, 60.f), FColor::White);

	// Sense shapes
	if (Data.bDrawSenseShapes)
	{
		for (const FArcPerceptionDebugSenseShape& Shape : Data.SenseShapes)
		{
			if (Shape.Type == FArcPerceptionDebugSenseShape::EType::Sphere)
			{
				StoredSpheres.Emplace(Shape.Radius, Shape.Origin, Shape.Color);
			}
			else // Cone
			{
				const float ConeRadius = Shape.ConeLength * FMath::Tan(FMath::DegreesToRadians(Shape.ConeHalfAngleDegrees));
				const FVector ConeEnd = Shape.Origin + Shape.Forward * Shape.ConeLength;
				const FVector Right = FVector::CrossProduct(Shape.Forward, FVector::UpVector).GetSafeNormal();
				const FVector Up = FVector::CrossProduct(Right, Shape.Forward);

				constexpr int32 ConeSegments = 16;
				const float AngleStep = 2.f * PI / static_cast<float>(ConeSegments);

				TArray<FVector, TInlineAllocator<17>> RingPoints;
				RingPoints.SetNum(ConeSegments + 1);
				for (int32 i = 0; i <= ConeSegments; ++i)
				{
					const float Angle = AngleStep * static_cast<float>(i);
					RingPoints[i] = ConeEnd + Right * (FMath::Cos(Angle) * ConeRadius) + Up * (FMath::Sin(Angle) * ConeRadius);
				}

				for (int32 i = 0; i < ConeSegments; i += 4)
				{
					StoredLines.Emplace(Shape.Origin, RingPoints[i], Shape.Color, 1.5f);
				}
				for (int32 i = 0; i < ConeSegments; ++i)
				{
					StoredLines.Emplace(RingPoints[i], RingPoints[i + 1], Shape.Color, 1.5f);
				}
			}
		}
	}

	// Perceived entities
	if (Data.bDrawPerceivedPositions)
	{
		for (const FArcPerceptionDebugPerceivedEntity& PE : Data.PerceivedEntities)
		{
			StoredSpheres.Emplace(PE.SphereRadius, PE.Location + FVector(0, 0, PE.ZOffset), PE.Color);
			StoredTexts.Emplace(PE.Label, PE.Location + FVector(0, 0, PE.ZOffset + PE.SphereRadius + 20.f), PE.Color);
			if (PE.bHighlighted)
			{
				StoredLines.Emplace(PE.Location, PE.Location + FVector(0, 0, PE.ZOffset + PE.SphereRadius + 10.f), PE.Color, 2.f);
			}
		}
	}
}
