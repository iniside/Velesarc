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

#include "Experience/ArcMassExperienceManagerComponent.h"
#include "ModularGameState.h"

#include "ArcMassGameState.generated.h"

class APlayerState;

/**
 * AArcMassGameState
 *
 * Modular GameState for the Mass-driven game framework. Intentionally has NO
 * AbilitySystemComponent and does NOT implement IAbilitySystemInterface.
 * The experience manager component will be attached in step 9.
 */
UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API AArcMassGameState : public AModularGameStateBase
{
	GENERATED_BODY()

public:
	AArcMassGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	float GetServerFPS() const
	{
		return ServerFPS;
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated)
	float ServerFPS;

	/** Drives experience selection, asset loading, GameFeature plugin activation and action execution. */
	UPROPERTY()
	TObjectPtr<UArcMassExperienceManagerComponent> ExperienceManagerComponent;
};
