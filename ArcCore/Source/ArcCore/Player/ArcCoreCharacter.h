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

#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "ModularCharacter.h"
#include "ArcCoreCharacter.generated.h"

class UGameplayCameraComponent;
class UGameplayCameraSystemComponent;
class UGameplayCameraSystemHost;
class IGameplayCameraSystemHost;

DECLARE_LOG_CATEGORY_EXTERN(LogArcCoreCharacter, Log, Log);

UCLASS(Abstract)
class ARCCORE_API AArcCoreCharacter : public AModularCharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(Replicated)
	FVector LocalViewPoint = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FRotator LocalViewRotation = FRotator::ZeroRotator;

	IGameplayCameraSystemHost* GameplayCameraSystemHost;
	
	UFUNCTION(Unreliable, Server)
	void ServerSetViewPointAndRotation(FVector NewViewPoint, FRotator NewViewRotation);

	UFUNCTION(BlueprintImplementableEvent)
	void OnPlayerControllerReplicated(APlayerController* PC);
	
public:
	AArcCoreCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void PossessedBy(AController* NewController) override;

	virtual void UnPossessed() override;

	virtual void OnRep_Controller() override;

	virtual void OnRep_PlayerState() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void BeginPlay() override;
	virtual void BecomeViewTarget(APlayerController* PC) override;
	virtual void Tick(float DeltaTime) override;
		
public:
	virtual void PreInitializeComponents() override;

	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Character")
	UArcCoreAbilitySystemComponent* GetArcAbilitySystemComponent() const;
	
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
};
