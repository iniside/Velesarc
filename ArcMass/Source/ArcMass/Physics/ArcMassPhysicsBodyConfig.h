// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
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
	/** Copy only serialized (UPROPERTY, non-transient) properties from Source to Dest.
	 *  Skips runtime state: ActorHandle, OwnerComponent, WeldParent, DOFConstraint, etc. */
	ARCMASS_API void CopyBodyInstanceConfig(FBodyInstance& Dest, const FBodyInstance& Source);

	/** Mobile-vis entity transforms are the feet, so their centred trait bodies are lifted by this to rest on them. */
	ARCMASS_API double GetFeetToBodyOffsetZ(const FArcMassPhysicsBodyConfigFragment& Config);

	ARCMASS_API void InitBodiesFromConfig(
		const FArcMassPhysicsBodyConfigFragment& Config,
		TConstArrayView<FTransform> Transforms,
		FPhysScene* PhysScene,
		TArray<FBodyInstance*>& OutBodies);
}
