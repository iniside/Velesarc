// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassPhysicsBodyConfig.h"
#include "ArcCollisionGeometry.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ArcMassPhysicsBodyTrait.generated.h"

class UBodySetup;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Physics Body", Category = "Physics"))
class ARCMASS_API UArcMassPhysicsBodyTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	bool HasCollisionGeometry() const;

#if WITH_EDITOR
	const FKAggregateGeom& GetCollisionGeometry() const { return CollisionGeometry.AggGeom; }
	FArcCollisionGeometry& GetMutableCollisionGeometry() { return CollisionGeometry; }
	void ValidateCollisionGeometry() const;
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	EArcMassPhysicsBodyType PhysicsBodyType = EArcMassPhysicsBodyType::Static;

	UPROPERTY(EditAnywhere, Category = "Physics")
	FArcCollisionGeometry CollisionGeometry;

	UPROPERTY(EditAnywhere, Category = "Physics", meta = (ShowOnlyInnerProperties, SkipUCSModifiedProperties))
	FBodyInstance BodyTemplate;
};
