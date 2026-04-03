// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBodyTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "ArcMassPhysicsBody.h"
#include "PhysicsEngine/BodySetup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassPhysicsBodyTrait)

void UArcMassPhysicsBodyTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	if (!HasCollisionGeometry())
	{
		UE_LOG(LogMass, Warning, TEXT("UArcMassPhysicsBodyTrait: No collision shapes configured on '%s'. Physics body will not be created."),
			*GetName());
		return;
	}

#if WITH_EDITOR
	ValidateCollisionGeometry();
#endif

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	UBodySetup* NewBodySetup = NewObject<UBodySetup>(const_cast<UArcMassPhysicsBodyTrait*>(this), NAME_None, RF_NoFlags);
	NewBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	NewBodySetup->AggGeom = CollisionGeometry.AggGeom;
	NewBodySetup->CreatePhysicsMeshes();

	FArcMassPhysicsBodyConfigFragment PhysicsConfig;
	PhysicsConfig.BodySetup = NewBodySetup;
	PhysicsConfig.BodyTemplate = BodyTemplate;
	PhysicsConfig.BodyType = PhysicsBodyType;
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(PhysicsConfig));

	BuildContext.AddFragment<FArcMassPhysicsBodyFragment>();
}

#if WITH_EDITOR
void UArcMassPhysicsBodyTrait::ValidateCollisionGeometry() const
{
	const FKAggregateGeom& Geom = CollisionGeometry.AggGeom;
	const FString TraitName = GetName();

	for (int32 Idx = 0; Idx < Geom.SphereElems.Num(); ++Idx)
	{
		if (Geom.SphereElems[Idx].Radius <= UE_KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogMass, Warning, TEXT("UArcMassPhysicsBodyTrait '%s': Sphere[%d] has zero or near-zero radius."), *TraitName, Idx);
		}
	}

	for (int32 Idx = 0; Idx < Geom.BoxElems.Num(); ++Idx)
	{
		const FKBoxElem& Box = Geom.BoxElems[Idx];
		if (Box.X <= UE_KINDA_SMALL_NUMBER || Box.Y <= UE_KINDA_SMALL_NUMBER || Box.Z <= UE_KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogMass, Warning, TEXT("UArcMassPhysicsBodyTrait '%s': Box[%d] has zero or near-zero extent (%.2f, %.2f, %.2f)."),
				*TraitName, Idx, Box.X, Box.Y, Box.Z);
		}
	}

	for (int32 Idx = 0; Idx < Geom.SphylElems.Num(); ++Idx)
	{
		const FKSphylElem& Sphyl = Geom.SphylElems[Idx];
		if (Sphyl.Radius <= UE_KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogMass, Warning, TEXT("UArcMassPhysicsBodyTrait '%s': Capsule[%d] has zero or near-zero radius."), *TraitName, Idx);
		}
	}

	for (int32 Idx = 0; Idx < Geom.ConvexElems.Num(); ++Idx)
	{
		if (Geom.ConvexElems[Idx].VertexData.Num() < 4)
		{
			UE_LOG(LogMass, Warning, TEXT("UArcMassPhysicsBodyTrait '%s': Convex[%d] has fewer than 4 vertices."), *TraitName, Idx);
		}
	}
}
#endif

bool UArcMassPhysicsBodyTrait::HasCollisionGeometry() const
{
	return CollisionGeometry.AggGeom.BoxElems.Num() > 0
		|| CollisionGeometry.AggGeom.SphereElems.Num() > 0
		|| CollisionGeometry.AggGeom.SphylElems.Num() > 0
		|| CollisionGeometry.AggGeom.ConvexElems.Num() > 0;
}
