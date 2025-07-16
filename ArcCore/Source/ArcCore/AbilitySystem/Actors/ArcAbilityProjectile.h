/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "ArcCore/AbilitySystem/Actors/ArcAbilityActor.h"
#include "ArcAbilityProjectile.generated.h"

/*
 * Base class for projectiles with base projectile movement component and no root.
 * The root component should be derived from any component that is inherited from
 * PrimitiveComponent.
 */
UCLASS()
class ARCCORE_API AArcAbilityProjectile : public AArcAbilityActor
{
	GENERATED_BODY()

public:
	static const FName BasePMCName;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UArcAbilityPMC> BasePMC2;

public:
	// Sets default values for this actor's properties
	AArcAbilityProjectile();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	void SetLocalVelocity(const FVector& Start
						  , const FVector& End
						  , float Speed);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	void SetVelocity(const FVector& InVelocity);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	void SetGravityScale(float InGravityScale);
};
