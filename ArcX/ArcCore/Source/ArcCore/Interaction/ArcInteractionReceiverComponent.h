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

#include "Components/ActorComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "Engine/HitResult.h"
#include "ArcInteractionReceiverComponent.generated.h"

class UArcItemsComponent;
class UArcInteractionLevelPlacedComponent;
class UArcInteractionAction;
class UCancellableAsyncAction;
class UArcInteractionOption;
class IArcInteractionObjectInterface;

/*
 * Component which receives possible interaction from InteractionAdvertiserComponent.
 */
UCLASS(Blueprintable, ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcInteractionReceiverComponent : public UActorComponent
{
	GENERATED_BODY()
	friend class UArcInteractionAction;

private:
	UPROPERTY(Transient)
	FGuid CurrentInteractionId;

	UPROPERTY(Transient)
	TScriptInterface<IArcInteractionObjectInterface> CurrentInteractionInterface;

	FVector CurrentInteractionLocation;
	
	UFUNCTION()
	void OnRep_CurrentTarget(AActor* OldTarget);
	
	UPROPERTY()
	TObjectPtr<APawn> InstigatorPawn;
	
	UPROPERTY()
	TObjectPtr<UArcInteractionLevelPlacedComponent> CurrentInteractionComponent;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcInteractionSearch", ExcludeBaseStruct))
	FInstancedStruct SearchType;

	// Need to save it, so we can give option chance for Cleanup, when changing interaction target.
	TWeakObjectPtr<UArcInteractionOption> CurrentOption = nullptr;

	FInstancedStruct OptionInstancedData;
	
public:
	const FGuid& GetCurrentInteractionId() const
	{
		return CurrentInteractionId;
	}
	
	APawn* GetInstigatorPawn() const
	{
		return InstigatorPawn;
	}

	void StartInteraction(TScriptInterface<IArcInteractionObjectInterface> InteractionManager, const FHitResult& InInteractionHit);
	void EndInteraction();
		
	UArcInteractionOption* GetCurrentOption() const
	{
		return CurrentOption.Get();
	}
	
	UFUNCTION(BlueprintPure, Category = "Arc Core|Interaction")
	UArcInteractionLevelPlacedComponent* GetCurrentInteractionComponent() const
	{
		return CurrentInteractionComponent;
	}

	UArcInteractionReceiverComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	void InitializeOptionInstanceData(const UScriptStruct* InInstanceDataType);
	
public:
	template<typename T>
	T* GetOptionInstanceData()
	{
		return OptionInstancedData.GetMutablePtr<T>();
	}
};
