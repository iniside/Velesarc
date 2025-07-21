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

#include "Components/GameStateComponent.h"
#include "GameFramework/OnlineReplStructs.h"

#include "ArcPlayerSpawningManagerComponent.generated.h"

class AController;
class APlayerController;
class APlayerState;
class APlayerStart;
class AArcPlayerStart;
class AActor;

/**
 * @class UArcPlayerSpawningManagerComponent
 */
UCLASS()
class ARCCORE_API UArcPlayerSpawningManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UArcPlayerSpawningManagerComponent(const FObjectInitializer& ObjectInitializer);

	/** UActorComponent */
	virtual void InitializeComponent() override;

	virtual void TickComponent(float DeltaTime
							   , enum ELevelTick TickType
							   , FActorComponentTickFunction* ThisTickFunction) override;

	/** ~UActorComponent */

protected:
	// Utility
	APlayerStart* GetFirstRandomUnoccupiedPlayerStart(AController* Controller
													  , const TArray<AArcPlayerStart*>& FoundStartPoints) const;

	virtual AActor* OnChoosePlayerStart(AController* Player
										, TArray<AArcPlayerStart*>& PlayerStarts)
	{
		return nullptr;
	}

	virtual void OnFinishRestartPlayer(AController* Player
									   , const FRotator& StartRotation)
	{
	}

	UFUNCTION(BlueprintImplementableEvent
		, meta = (DisplayName = OnFinishRestartPlayer))
	void K2_OnFinishRestartPlayer(AController* Player
								  , const FRotator& StartRotation);

public:
	/** We proxy these calls from AArcGameMode, to this component so that each experience
	 * can more easily customize the respawn system they want. */
	AActor* ChoosePlayerStart(AController* Player);

	bool PlayerCanRestart(APlayerController* Player);

	void FinishRestartPlayer(AController* NewPlayer
							 , const FRotator& StartRotation);

	/** ~AArcGameMode */

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AArcPlayerStart>> CachedPlayerStarts;

private:
	void OnLevelAdded(ULevel* InLevel
					  , UWorld* InWorld);

	void HandleOnActorSpawned(AActor* SpawnedActor);

#if WITH_EDITOR
	APlayerStart* FindPlayFromHereStart(AController* Player);
#endif
};
