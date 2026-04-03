// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "ArcCollisionGeometry.generated.h"

USTRUCT()
struct ARCMASS_API FArcCollisionGeometry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Collision")
	FKAggregateGeom AggGeom;
};
