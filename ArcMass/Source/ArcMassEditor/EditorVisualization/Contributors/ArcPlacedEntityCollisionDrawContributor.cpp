// Copyright Lukasz Baran. All Rights Reserved.

#include "EditorVisualization/Contributors/ArcPlacedEntityCollisionDrawContributor.h"
#include "EditorVisualization/ArcEditorDrawCollector.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyTrait.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/ConvexElem.h"

namespace ArcMassEditor_CollisionDraw
{

static void DrawSpheres(
	FArcEditorDrawCollector& Collector,
	const TArray<FKSphereElem>& Elems,
	const FTransform& InstanceWorld,
	const FColor& Color)
{
	for (const FKSphereElem& Elem : Elems)
	{
		// FKSphereElem::GetTransform() returns FTransform(Center) — translation only, no rotation.
		// Scale is inherited from InstanceWorld; use GetMax() for a uniform-scale approximation
		// matching the engine's GetScaledRadius convention (GetAbsMin for physics, GetMax for vis).
		const FTransform FinalTM = Elem.GetTransform() * InstanceWorld;
		const float ScaledRadius = Elem.Radius * FinalTM.GetScale3D().GetMax();
		Collector.AddSphere(FinalTM.GetLocation(), ScaledRadius, Color);
	}
}

static void DrawBoxes(
	FArcEditorDrawCollector& Collector,
	const TArray<FKBoxElem>& Elems,
	const FTransform& InstanceWorld,
	const FColor& Color)
{
	for (const FKBoxElem& Elem : Elems)
	{
		// FKBoxElem::GetTransform() returns FTransform(Rotation, Center).
		// X/Y/Z are FULL side lengths; half-extents are X/2, Y/2, Z/2, scaled per-axis.
		const FTransform FinalTM = Elem.GetTransform() * InstanceWorld;
		const FVector Scale = FinalTM.GetScale3D();
		const FVector HalfExtent = FVector(Elem.X * 0.5f * Scale.X, Elem.Y * 0.5f * Scale.Y, Elem.Z * 0.5f * Scale.Z);
		Collector.AddBox(FinalTM.GetLocation(), HalfExtent, FinalTM.GetRotation(), Color);
	}
}

static void DrawCapsules(
	FArcEditorDrawCollector& Collector,
	const TArray<FKSphylElem>& Elems,
	const FTransform& InstanceWorld,
	const FColor& Color)
{
	for (const FKSphylElem& Elem : Elems)
	{
		// FKSphylElem::Length is the CYLINDER SEGMENT length ("line-segment length; add Radius to
		// both ends to find total length" — SphylElem.h). FArcEditorDrawCollector::AddCapsule takes
		// HalfHeight as the half cylinder-segment length; the dome hemispheres are drawn beyond that
		// by the DebugDraw plugin. So HalfHeight = Length * 0.5, scaled by the Z scale component
		// (capsule axis is Z per SphylElem.h: "Z axis is capsule axis").
		// Radius is scaled by the XY max (matching GetScaledRadius convention).
		const FTransform FinalTM = Elem.GetTransform() * InstanceWorld;
		const FVector Scale = FinalTM.GetScale3D();
		const float ScaledHalfHeight = Elem.Length * 0.5f * FMath::Abs(Scale.Z);
		const float ScaledRadius = Elem.Radius * FMath::Max(FMath::Abs(Scale.X), FMath::Abs(Scale.Y));
		Collector.AddCapsule(FinalTM.GetLocation(), ScaledHalfHeight, ScaledRadius, FinalTM.GetRotation(), Color);
	}
}

static void DrawConvexHulls(
	FArcEditorDrawCollector& Collector,
	const TArray<FKConvexElem>& Elems,
	const FTransform& InstanceWorld,
	const FColor& Color)
{
	for (const FKConvexElem& Elem : Elems)
	{
		const TArray<FVector>& Verts = Elem.VertexData;
		const TArray<int32>& Indices = Elem.IndexData;

		if (Verts.IsEmpty() || Indices.IsEmpty())
		{
			continue;
		}

		const FTransform FinalTM = Elem.GetTransform() * InstanceWorld;

		// Deduplicate edges by packing the sorted vertex index pair into a uint64.
		// Key = (uint64)Min(i,j) << 32 | (uint64)Max(i,j).
		// This avoids comparing transformed FVector positions and is O(1) per lookup.
		TSet<uint64> SeenEdges;
		const int32 TriCount = Indices.Num() / 3;
		SeenEdges.Reserve(TriCount * 3);

		for (int32 TriIdx = 0; TriIdx < TriCount; ++TriIdx)
		{
			const int32 BaseIdx = TriIdx * 3;
			const int32 IdxA = Indices[BaseIdx + 0];
			const int32 IdxB = Indices[BaseIdx + 1];
			const int32 IdxC = Indices[BaseIdx + 2];

			// Triangle edges: (A,B), (B,C), (C,A)
			int32 EdgePairs[3][2] = { {IdxA, IdxB}, {IdxB, IdxC}, {IdxC, IdxA} };
			for (int32 EdgeIdx = 0; EdgeIdx < 3; ++EdgeIdx)
			{
				const int32 Lo = FMath::Min(EdgePairs[EdgeIdx][0], EdgePairs[EdgeIdx][1]);
				const int32 Hi = FMath::Max(EdgePairs[EdgeIdx][0], EdgePairs[EdgeIdx][1]);
				const uint64 Key = (static_cast<uint64>(Lo) << 32) | static_cast<uint64>(Hi);

				bool bAlreadyIn = false;
				SeenEdges.Add(Key, &bAlreadyIn);
				if (bAlreadyIn)
				{
					continue;
				}

				if (!Verts.IsValidIndex(Lo) || !Verts.IsValidIndex(Hi))
				{
					continue;
				}

				const FVector P0 = FinalTM.TransformPosition(Verts[Lo]);
				const FVector P1 = FinalTM.TransformPosition(Verts[Hi]);
				Collector.AddLine(P0, P1, Color);
			}
		}
	}
}

} // namespace ArcMassEditor_CollisionDraw

TSubclassOf<UMassEntityTraitBase> FArcPlacedEntityCollisionDrawContributor::GetTraitClass() const
{
	return UArcMassPhysicsBodyTrait::StaticClass();
}

void FArcPlacedEntityCollisionDrawContributor::CollectShapes(
	FArcEditorDrawCollector& Collector,
	const UMassEntityTraitBase* Trait,
	TConstArrayView<FTransform> Transforms,
	const FArcEditorDrawContext& Context)
{
	const UArcMassPhysicsBodyTrait* PhysicsTrait = Cast<UArcMassPhysicsBodyTrait>(Trait);
	if (!PhysicsTrait || !PhysicsTrait->HasCollisionGeometry())
	{
		return;
	}

	const FKAggregateGeom& Agg = PhysicsTrait->GetCollisionGeometry();
	const FColor Color = FColor::Green;

	for (const FTransform& InstanceWorld : Transforms)
	{
		ArcMassEditor_CollisionDraw::DrawSpheres(Collector, Agg.SphereElems, InstanceWorld, Color);
		ArcMassEditor_CollisionDraw::DrawBoxes(Collector, Agg.BoxElems, InstanceWorld, Color);
		ArcMassEditor_CollisionDraw::DrawCapsules(Collector, Agg.SphylElems, InstanceWorld, Color);
		ArcMassEditor_CollisionDraw::DrawConvexHulls(Collector, Agg.ConvexElems, InstanceWorld, Color);
	}
}
