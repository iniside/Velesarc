// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAoETypes.h"

#include "ArcScalableFloatItemFragment_TargetingShape.h"
#include "ArcItemFragment_TargetingShapeConfig.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

FArcAoEShapeData FArcAoEShapeData::FromItemData(const FArcItemData* ItemData)
{
	FArcAoEShapeData Result;

	if (!ItemData)
	{
		return Result;
	}

	const FArcItemFragment_TargetingShapeConfig* ShapeConfig = ArcItemsHelper::FindFragment<FArcItemFragment_TargetingShapeConfig>(ItemData);

	if (const FArcScalableFloatItemFragment_TargetingShape* ShapeFrag = ItemData->FindScalableItemFragment<FArcScalableFloatItemFragment_TargetingShape>())
	{
		const float ShapeRadius = FArcScalableFloatItemFragment_TargetingShape::GetRadiusValue(ItemData);
		const float ShapeLength = FArcScalableFloatItemFragment_TargetingShape::GetLengthValue(ItemData);
		const float ShapeWidth = FArcScalableFloatItemFragment_TargetingShape::GetWidthValue(ItemData);
		const float ShapeHeight = FArcScalableFloatItemFragment_TargetingShape::GetHeightValue(ItemData);

		if (ShapeConfig && ShapeConfig->Shape == EArcAoEShape::Box)
		{
			Result.Shape = EArcAoEShape::Box;
			Result.BoxAlignment = ShapeConfig->BoxAlignment;

			const float HalfLength = ShapeLength * 0.5f;
			const float HalfWidth = ShapeWidth * 0.5f;
			const float HalfHeight = ShapeHeight * 0.5f;

			switch (Result.BoxAlignment)
			{
			case EArcAoEBoxAlignment::ShortEdgeFacingSource:
				Result.BoxExtent = FVector(HalfLength, HalfWidth, HalfHeight);
				break;
			case EArcAoEBoxAlignment::LongEdgeFacingSource:
			default:
				Result.BoxExtent = FVector(HalfWidth, HalfLength, HalfHeight);
				break;
			}
		}
		else
		{
			Result.Shape = EArcAoEShape::Sphere;
			Result.Radius = ShapeRadius;
		}
	}

	return Result;
}

void FArcAoEShapeData::ComputeBoxTransform(const FVector& SourceLocation, const FVector& Direction, FVector& OutCenter, FQuat& OutRotation) const
{
	FVector FlatDirection = Direction;
	FlatDirection.Z = 0.0f;

	if (FlatDirection.IsNearlyZero())
	{
		OutCenter = SourceLocation;
		OutRotation = FQuat::Identity;
		return;
	}

	FlatDirection.Normalize();

	FRotator YawRotation = FlatDirection.Rotation();
	OutRotation = YawRotation.Quaternion();
	OutCenter = SourceLocation + (FlatDirection * BoxExtent.X);
}
