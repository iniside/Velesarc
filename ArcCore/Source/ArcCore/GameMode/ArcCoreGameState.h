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

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "ModularGameState.h"
#include "ArcCoreGameState.generated.h"

class UArcExperienceManagerComponent;
class UArcCoreAbilitySystemComponent;
class UAbilitySystemComponent;

/**
 *
 */
UCLASS()
class ARCCORE_API AArcCoreGameState
		: public AModularGameStateBase
		, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArcCoreGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	float GetServerFPS() const
	{
		return ServerFPS;
	}

	//~AActor interface
	virtual void PreInitializeComponents() override;

	virtual void PostInitializeComponents() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//~End of AActor interface

	//~AGameStateBase interface
	virtual void AddPlayerState(APlayerState* PlayerState) override;

	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	//~End of AGameStateBase interface

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//~End of IAbilitySystemInterface

	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|GameState")
	UArcCoreAbilitySystemComponent* GetArcAbilitySystemComponent() const
	{
		return AbilitySystemComponent;
	}

	// Send a message that all clients will (probably) get
	// (use only for client notifications like eliminations, server join messages, etc...
	// that can handle being lost) UFUNCTION(NetMulticast, Unreliable, BlueprintCallable,
	// Category = "Arc Core|GameState")
	//	void MulticastMessageToClients(const FArcVerbMessage Message);

private:
	UPROPERTY()
	TObjectPtr<UArcExperienceManagerComponent> ExperienceManagerComponent;

	// The ability system component subobject for game-wide things (primarily gameplay
	// cues)
	UPROPERTY(VisibleAnywhere
		, Category = "Arc Core|GameState")
	TObjectPtr<UArcCoreAbilitySystemComponent> AbilitySystemComponent;

protected:
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(Replicated)
	float ServerFPS;
};
