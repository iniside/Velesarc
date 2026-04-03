/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "IndicatorDescriptor.h"
#include "Engine/LocalPlayer.h"
#include "MassEntityManager.h"
#include "Mass/EntityFragments.h"

bool FActorCanvasProjection::Project(FIndicatorDescriptor& CanvasEntry, const FSceneViewProjectionData& InProjectionData, const FVector2f& ScreenSize, FVector& OutScreenPositionWithDepth, const FMassEntityManager* EntityManager)
{
	TOptional<FVector> WorldLocation;

	switch (CanvasEntry.LocationMode)
	{
		case EIndicatorLocationMode::Component:
		{
			USceneComponent* Component = CanvasEntry.Component.Get();
			if (!Component)
			{
				return false;
			}

			if (CanvasEntry.ComponentSocketName != NAME_None)
			{
				WorldLocation = Component->GetSocketTransform(CanvasEntry.ComponentSocketName).GetLocation();
			}
			else
			{
				WorldLocation = Component->GetComponentLocation();
			}

			if (CanvasEntry.LocationInterfaceActor.IsValid())
			{
				WorldLocation = IArcIndicatorLocationInterface::Execute_GetIndicatorLocation(CanvasEntry.LocationInterfaceActor.Get());
			}
			break;
		}
		case EIndicatorLocationMode::MassEntity:
		{
			if (!EntityManager)
			{
				return false;
			}
			if (!EntityManager->IsEntityValid(CanvasEntry.MassEntity))
			{
				return false;
			}
			const FTransformFragment* TransformFragment = EntityManager->GetFragmentDataPtr<FTransformFragment>(CanvasEntry.MassEntity);
			if (!TransformFragment)
			{
				return false;
			}
			WorldLocation = TransformFragment->GetTransform().GetLocation();
			break;
		}
		case EIndicatorLocationMode::ManualTransform:
		{
			WorldLocation = CanvasEntry.ManualWorldLocation;
			break;
		}
	}

	if (!WorldLocation.IsSet())
	{
		return false;
	}

	FVector ProjectWorldLocation = WorldLocation.GetValue() + CanvasEntry.GetWorldPositionOffset();

	switch (CanvasEntry.GetProjectionMode())
	{
		case EActorCanvasProjectionMode::Point:
		{
			FVector2D OutScreenSpacePosition;
			if (ULocalPlayer::GetPixelPoint(InProjectionData, ProjectWorldLocation, OutScreenSpacePosition, &ScreenSize))
			{
				OutScreenSpacePosition += CanvasEntry.GetScreenSpaceOffset();

				OutScreenPositionWithDepth = FVector(OutScreenSpacePosition.X, OutScreenSpacePosition.Y, FVector::Dist(InProjectionData.ViewOrigin, ProjectWorldLocation));
				return true;
			}
			return false;
		}
		case EActorCanvasProjectionMode::BoundingBox:
		{
			if (CanvasEntry.LocationMode != EIndicatorLocationMode::Component)
			{
				return false;
			}
			USceneComponent* Component = CanvasEntry.Component.Get();
			if (!Component)
			{
				return false;
			}

			FBox IndicatorBox;
			if (Component == Component->GetOwner()->GetRootComponent())
			{
				IndicatorBox = Component->GetOwner()->GetComponentsBoundingBox();
			}
			else
			{
				IndicatorBox = Component->Bounds.GetBox();
			}

			FVector2D LL, UR;
			if (ULocalPlayer::GetPixelBoundingBox(InProjectionData, IndicatorBox, LL, UR, &ScreenSize))
			{
				OutScreenPositionWithDepth.X = FMath::Lerp(LL.X, UR.X, CanvasEntry.GetBoundingBoxAnchor().X) + CanvasEntry.GetScreenSpaceOffset().X;
				OutScreenPositionWithDepth.Y = FMath::Lerp(LL.Y, UR.Y, CanvasEntry.GetBoundingBoxAnchor().Y) + CanvasEntry.GetScreenSpaceOffset().Y;
				OutScreenPositionWithDepth.Z = FVector::Dist(InProjectionData.ViewOrigin, ProjectWorldLocation);
				return true;
			}
			return false;
		}
	}

	return false;
}