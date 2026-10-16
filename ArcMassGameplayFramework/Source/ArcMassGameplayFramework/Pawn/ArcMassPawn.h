/**
 * This file is part of Velesarc
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

#include "GameFramework/Pawn.h"
#include "ArcMassPawn.generated.h"

class IGameplayCameraSystemHost;
class UCharacterMoverComponent;
class UNavMoverComponent;
class UGameplayCameraComponent;
class UCapsuleComponent;

UCLASS(Abstract)
class ARCMASSGAMEPLAYFRAMEWORK_API AArcMassPawn : public APawn
{
	GENERATED_BODY()

public:
	AArcMassPawn(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintPure, Category = Mover)
	UCharacterMoverComponent* GetMoverComponent() const { return CharacterMotionComponent; }

	UFUNCTION(BlueprintCallable, Category=MoverExamples)
	virtual void RequestMoveByIntent(const FVector& DesiredIntent) { CachedMoveInputIntent = DesiredIntent; }

	UFUNCTION(BlueprintCallable, Category=MoverExamples)
	virtual void RequestMoveByVelocity(const FVector& DesiredVelocity) { CachedMoveInputVelocity = DesiredVelocity; }

	//~ Begin INavAgentInterface Interface
	virtual FVector GetNavAgentLocation() const override;
	//~ End INavAgentInterface Interface

	virtual void UpdateNavigationRelevance() override;

protected:
	UPROPERTY(Category = Movement, VisibleAnywhere, BlueprintReadOnly, Transient, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterMoverComponent> CharacterMotionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Nav Movement")
	TObjectPtr<UNavMoverComponent> NavMoverComponent;

private:
	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FVector CachedMoveInputVelocity = FVector::ZeroVector;

	FRotator CachedLookInput = FRotator::ZeroRotator;
};

UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API AArcMassPlayerPawn : public AArcMassPawn
{
	GENERATED_BODY()

public:
	AArcMassPlayerPawn(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY()
	mutable TObjectPtr<UCapsuleComponent> CachedCapsuleComponent;

	UPROPERTY()
	mutable TObjectPtr<UGameplayCameraComponent> CachedGameplayCameraComponent;

	IGameplayCameraSystemHost* GameplayCameraSystemHost = nullptr;

	UPROPERTY(Transient, Replicated)
	FVector LocalViewPoint = FVector::ZeroVector;

	UPROPERTY(Transient, Replicated)
	FRotator LocalViewRotation = FRotator::ZeroRotator;

public:
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	UFUNCTION(Server, Unreliable)
	void ServerSetViewPointAndRotation(FVector_NetQuantize InViewPoint, FRotator InViewRotation);

	UCapsuleComponent* GetCapsuleComponent() const;

	UGameplayCameraComponent* GetGameplayCameraComponent() const;
};
