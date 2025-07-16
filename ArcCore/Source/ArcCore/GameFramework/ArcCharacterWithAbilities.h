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
#include "GameplayTagAssetInterface.h"
#include "ModularCharacter.h"
#include "ArcCharacterWithAbilities.generated.h"

/**
 * Base class implementing ability system. Otherwise does not have any components created
 * by default.
 */
UCLASS()
class ARCCORE_API AArcCharacterWithAbilities
		: public AModularCharacter
		, public IAbilitySystemInterface
		, public IGameplayTagAssetInterface

{
	GENERATED_BODY()

protected:
	static const FName AbilitySystemName;
	static const FName PawnExtenstionName;
	
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<class UArcCoreAbilitySystemComponent> AbilitySystem;

public:
	AArcCharacterWithAbilities(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void PossessedBy(AController* NewController) override;

	virtual void UnPossessed() override;

public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
};
