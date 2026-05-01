// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcVisSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ArcMass Visualization"))
class ARCMASS_API UArcVisSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName("ArcMass"); }
	virtual FName GetSectionName() const override { return FName("Visualization"); }

	// --- Mesh Grid ---

	/** Grid cell size for mesh (ISM) activation in world units. */
	UPROPERTY(config, EditAnywhere, Category = "Mesh Grid", meta = (ClampMin = "1.0"))
	float MeshCellSize = 10000.f;

	/** Radius at which ISM instances are created. */
	UPROPERTY(config, EditAnywhere, Category = "Mesh Grid", meta = (ClampMin = "0.0"))
	float MeshActivationRadius = 20000.f;

	/** Radius at which ISM instances are destroyed. Must be >= MeshActivationRadius to prevent flickering. */
	UPROPERTY(config, EditAnywhere, Category = "Mesh Grid", meta = (ClampMin = "0.0"))
	float MeshDeactivationRadius = 25000.f;

	// --- Physics Grid ---

	/** Grid cell size for physics body activation in world units. */
	UPROPERTY(config, EditAnywhere, Category = "Physics Grid", meta = (ClampMin = "1.0"))
	float PhysicsCellSize = 7500.f;

	/** Radius at which physics bodies are created. */
	UPROPERTY(config, EditAnywhere, Category = "Physics Grid", meta = (ClampMin = "0.0"))
	float PhysicsActivationRadius = 15000.f;

	/** Radius at which physics bodies are destroyed. Must be >= PhysicsActivationRadius to prevent flickering. */
	UPROPERTY(config, EditAnywhere, Category = "Physics Grid", meta = (ClampMin = "0.0"))
	float PhysicsDeactivationRadius = 18000.f;
};
