// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ArcMassPhysicsBody.h"
#include "PhysicsInterfaceDeclaresCore.h"
#include "ArcMassPhysicsBodyConfig.generated.h"

class UBodySetup;

USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassPhysicsBodyConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Physics", meta=(ShowOnlyInnerProperties))
	FBodyInstance BodyTemplate;

	UPROPERTY(EditAnywhere, Category = "Physics")
	TObjectPtr<UBodySetup> BodySetup = nullptr;

	UPROPERTY(EditAnywhere, Category = "Physics")
	EArcMassPhysicsBodyType BodyType = EArcMassPhysicsBodyType::Static;
};

template<>
struct TMassFragmentTraits<FArcMassPhysicsBodyConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

namespace UE::ArcMass::Physics
{
	ARCMASS_API void InitBodiesFromConfig(
		const FArcMassPhysicsBodyConfigFragment& Config,
		TConstArrayView<FTransform> Transforms,
		FPhysScene* PhysScene,
		TArray<FBodyInstance*>& OutBodies);
}
