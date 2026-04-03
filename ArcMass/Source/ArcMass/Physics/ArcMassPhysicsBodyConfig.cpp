// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBodyConfig.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassPhysicsBodyConfig)

void UE::ArcMass::Physics::CopyBodyInstanceConfig(FBodyInstance& Dest, const FBodyInstance& Source)
{
	const UScriptStruct* Struct = FBodyInstance::StaticStruct();
	for (FProperty* Prop = Struct->PropertyLink; Prop; Prop = Prop->PropertyLinkNext)
	{
		if (Prop->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}
		Prop->CopyCompleteValue_InContainer(&Dest, &Source);
	}
}

void UE::ArcMass::Physics::InitBodiesFromConfig(
	const FArcMassPhysicsBodyConfigFragment& Config,
	TConstArrayView<FTransform> Transforms,
	FPhysScene* PhysScene,
	TArray<FBodyInstance*>& OutBodies)
{
	const int32 Count = Transforms.Num();
	if (Count == 0 || !Config.BodySetup || !PhysScene)
	{
		return;
	}

	OutBodies.SetNum(Count);
	TArray<FBodyInstance*> Bodies;
	TArray<FTransform> TransformArray;
	Bodies.Reserve(Count);
	TransformArray.Reserve(Count);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FBodyInstance* Body = new FBodyInstance();
		CopyBodyInstanceConfig(*Body, Config.BodyTemplate);
		OutBodies[Index] = Body;
		Bodies.Add(Body);
		TransformArray.Add(Transforms[Index]);
	}

	const bool bStaticBody = (Config.BodyType == EArcMassPhysicsBodyType::Static);
	FInitBodySpawnParams SpawnParams(bStaticBody, /*bPhysicsTypeDeterminesSimulation=*/false);

	if (bStaticBody)
	{
		FInitBodiesHelper<true> Helper(Bodies, TransformArray, Config.BodySetup,
			nullptr, nullptr, PhysScene, SpawnParams, FPhysicsAggregateHandle());
		Helper.InitBodies();
	}
	else
	{
		FInitBodiesHelper<false> Helper(Bodies, TransformArray, Config.BodySetup,
			nullptr, nullptr, PhysScene, SpawnParams, FPhysicsAggregateHandle());
		Helper.InitBodies();
	}

	// Null out any bodies that failed initialization (no valid physics actor)
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FBodyInstance* Body = OutBodies[Index];
		if (Body && !FPhysicsInterface::IsValid(Body->GetPhysicsActor()))
		{
			delete Body;
			OutBodies[Index] = nullptr;
		}
	}
}
