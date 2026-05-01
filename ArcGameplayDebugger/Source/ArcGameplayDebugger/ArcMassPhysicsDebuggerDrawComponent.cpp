// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsDebuggerDrawComponent.h"

void UArcMassPhysicsDebuggerDrawComponent::UpdatePhysicsDebug(const FArcMassPhysicsDebugDrawData& InData)
{
	Data = InData;
	SetDrawLabels(false);
	RefreshShapes();
}

void UArcMassPhysicsDebuggerDrawComponent::CollectShapes()
{
	if (Data.bDrawHitLine)
	{
		StoredLines.Emplace(Data.ViewLocation, Data.HitPoint, FColor::Yellow, 1.f);
	}
	StoredSpheres.Emplace(15.f, Data.HitPoint, Data.HitSphereColor);

	if (Data.bDrawBounds)
	{
		FBox BoundsBox = FBox::BuildAABB(Data.EntityLocation, FVector(50.f));
		StoredBoxes.Emplace(BoundsBox, FColor::Cyan, FDebugRenderSceneProxy::EDrawType::Invalid, 2.f);
	}

	for (const FArcMassPhysicsDebugDrawData::FShapeSphere& Sphere : Data.CollisionSpheres)
	{
		StoredSpheres.Emplace(Sphere.Radius, Sphere.Center, Sphere.Color);
	}
	for (const FArcMassPhysicsDebugDrawData::FShapeBox& Box : Data.CollisionBoxes)
	{
		FBox ShapeBox = FBox::BuildAABB(FVector::ZeroVector, Box.Extent);
		FTransform BoxTransform(Box.Rotation, Box.Center);
		StoredBoxes.Emplace(ShapeBox, Box.Color, BoxTransform, FDebugRenderSceneProxy::EDrawType::Invalid, 1.f);
	}
	for (const FArcMassPhysicsDebugDrawData::FShapeCapsule& Capsule : Data.CollisionCapsules)
	{
		StoredCapsules.Emplace(
			Capsule.Center,
			Capsule.Radius,
			Capsule.X, Capsule.Y, Capsule.Z,
			Capsule.HalfHeight,
			FLinearColor(Capsule.Color),
			FDebugRenderSceneProxy::EDrawType::Invalid,
			1.f);
	}
	for (const FArcMassPhysicsDebugDrawData::FShapeLine& Line : Data.ConvexLines)
	{
		StoredLines.Emplace(Line.Start, Line.End, Line.Color, 1.f);
	}
}
