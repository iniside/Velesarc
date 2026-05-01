// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcPlacedEntityCollisionDrawComponent.h"

#if WITH_EDITOR

#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyTrait.h"
#include "MassEntityConfigAsset.h"
#include "DebugRenderSceneProxy.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedEntityCollisionDrawComponent)

// --- Always-visible scene proxy ---

#if UE_ENABLE_DEBUG_DRAWING
class FArcPlacedEntityCollisionSceneProxy final : public FDebugRenderSceneProxy
{
public:
	explicit FArcPlacedEntityCollisionSceneProxy(const UPrimitiveComponent& InComponent)
		: FDebugRenderSceneProxy(&InComponent)
	{
		DrawType = WireMesh;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = true;
		Result.bDynamicRelevance = true;
		Result.bSeparateTranslucency = Result.bNormalTranslucency = true;
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + FDebugRenderSceneProxy::GetAllocatedSize();
	}
};
#endif

namespace ArcPlacedEntityCollisionDraw
{

static TAutoConsoleVariable<bool> CVarShowPlacedEntityCollision(
	TEXT("arc.editor.ShowPlacedEntityCollision"),
	true,
	TEXT("Show collision wireframes for placed Mass entities in the editor viewport"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* /*CVar*/)
	{
		for (UArcPlacedEntityCollisionDrawComponent* Comp : TObjectRange<UArcPlacedEntityCollisionDrawComponent>())
		{
			if (Comp->IsRegistered())
			{
				Comp->MarkRenderStateDirty();
			}
		}
	}));

} // namespace ArcPlacedEntityCollisionDraw

UArcPlacedEntityCollisionDrawComponent::UArcPlacedEntityCollisionDrawComponent()
{
	SetIsVisualizationComponent(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bSelectable = false;
}

void UArcPlacedEntityCollisionDrawComponent::Populate(TConstArrayView<FArcPlacedEntityEntry> Entries)
{
	CachedCollisionData.Reset();
	CachedBounds.Init();

	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (Entry.EntityConfig == nullptr || Entry.InstanceTransforms.Num() == 0)
		{
			continue;
		}

		const UArcMassPhysicsBodyTrait* PhysicsTrait = Cast<UArcMassPhysicsBodyTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcMassPhysicsBodyTrait::StaticClass()));
		if (PhysicsTrait == nullptr || !PhysicsTrait->HasCollisionGeometry())
		{
			continue;
		}

		FArcPlacedEntityCollisionData& Data = CachedCollisionData.AddDefaulted_GetRef();
		Data.AggGeom = PhysicsTrait->GetCollisionGeometry();
		Data.InstanceTransforms = Entry.InstanceTransforms;

		for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
		{
			CachedBounds += Data.AggGeom.CalcAABB(InstanceTransform);
		}
	}

	MarkRenderStateDirty();
}

#if UE_ENABLE_DEBUG_DRAWING
FDebugRenderSceneProxy* UArcPlacedEntityCollisionDrawComponent::CreateDebugSceneProxy()
{
	if (!ArcPlacedEntityCollisionDraw::CVarShowPlacedEntityCollision.GetValueOnGameThread())
	{
		return nullptr;
	}

	if (CachedCollisionData.IsEmpty())
	{
		return nullptr;
	}

	FArcPlacedEntityCollisionSceneProxy* Proxy = new FArcPlacedEntityCollisionSceneProxy(*this);

	const FColor CollisionColor(0, 255, 0);
	const FLinearColor CollisionLinearColor(CollisionColor);
	const FTransform OwnerToWorld = GetComponentTransform();

	for (const FArcPlacedEntityCollisionData& Data : CachedCollisionData)
	{
		for (const FTransform& InstanceTransform : Data.InstanceTransforms)
		{
			// Spheres
			for (const FKSphereElem& Elem : Data.AggGeom.SphereElems)
			{
				FTransform ElemTransform = Elem.GetTransform() * InstanceTransform * OwnerToWorld;
				Proxy->Spheres.Emplace(
					Elem.Radius,
					ElemTransform.GetLocation(),
					CollisionLinearColor);
			}

			// Boxes
			for (const FKBoxElem& Elem : Data.AggGeom.BoxElems)
			{
				FTransform ElemTransform = Elem.GetTransform() * InstanceTransform * OwnerToWorld;
				FBox LocalBox(
					FVector(-Elem.X * 0.5f, -Elem.Y * 0.5f, -Elem.Z * 0.5f),
					FVector(Elem.X * 0.5f, Elem.Y * 0.5f, Elem.Z * 0.5f));
				Proxy->Boxes.Emplace(LocalBox, CollisionColor, ElemTransform);
			}

			// Capsules (Sphyl)
			for (const FKSphylElem& Elem : Data.AggGeom.SphylElems)
			{
				FTransform ElemTransform = Elem.GetTransform() * InstanceTransform * OwnerToWorld;
				FVector X = ElemTransform.GetUnitAxis(EAxis::X);
				FVector Y = ElemTransform.GetUnitAxis(EAxis::Y);
				FVector Z = ElemTransform.GetUnitAxis(EAxis::Z);
				Proxy->Capsules.Emplace(
					ElemTransform.GetLocation(),
					Elem.Radius,
					X, Y, Z,
					Elem.Length * 0.5f,
					CollisionLinearColor);
			}

			// Convex — draw triangle edges as lines
			for (const FKConvexElem& Elem : Data.AggGeom.ConvexElems)
			{
				FTransform ElemTransform = Elem.GetTransform() * InstanceTransform * OwnerToWorld;
				const TArray<FVector>& Verts = Elem.VertexData;
				const TArray<int32>& Indices = Elem.IndexData;
				for (int32 Idx = 0; Idx + 2 < Indices.Num(); Idx += 3)
				{
					FVector V0 = ElemTransform.TransformPosition(Verts[Indices[Idx]]);
					FVector V1 = ElemTransform.TransformPosition(Verts[Indices[Idx + 1]]);
					FVector V2 = ElemTransform.TransformPosition(Verts[Indices[Idx + 2]]);
					Proxy->Lines.Emplace(V0, V1, CollisionColor);
					Proxy->Lines.Emplace(V1, V2, CollisionColor);
					Proxy->Lines.Emplace(V2, V0, CollisionColor);
				}
			}
		}
	}

	return Proxy;
}
#endif

FBoxSphereBounds UArcPlacedEntityCollisionDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (CachedBounds.IsValid)
	{
		return FBoxSphereBounds(CachedBounds.TransformBy(LocalToWorld));
	}
	return Super::CalcBounds(LocalToWorld);
}

#endif // WITH_EDITOR
