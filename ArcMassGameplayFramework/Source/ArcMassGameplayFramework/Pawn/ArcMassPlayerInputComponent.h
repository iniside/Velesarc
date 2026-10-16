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

#include "Pawn/ArcMassPawnComponent.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassPlayerInputComponent.generated.h"

class AActor;
class UInputComponent;
struct FInputActionValue;

/**
 * UArcMassPlayerInputComponent
 *
 * Player-only input surface for Mass-driven pawns. Auto-spawned by
 * UArcMassPawnExtensionComponent on locally-controlled non-bot pawns when the
 * pawn enters DataInitialized; absent on server / AI / simulated proxies so
 * they pay no cost for unused input plumbing.
 */
UCLASS(ClassGroup = "ArcMass", meta = (BlueprintSpawnableComponent))
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassPlayerInputComponent : public UArcMassPawnComponent
{
	GENERATED_BODY()

public:
	UArcMassPlayerInputComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static UArcMassPlayerInputComponent* FindPlayerInputComponent(const AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<UArcMassPlayerInputComponent>() : nullptr;
	}

	UFUNCTION(BlueprintPure, Category = "Input")
	const FVector2D& GetMoveInputVector() const { return MoveInputVector; }
	bool IsReadyToBindInputs() const { return bReadyToBindInputs; }

	void InitializePlayerInput(UInputComponent* PlayerInputComponent);

	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Move_Completed(const FInputActionValue& InputActionValue);
	void Input_LookMouse(const FInputActionValue& InputActionValue);
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	void Input_Custom_Started(const FInputActionValue& InputActionValue, FGameplayTag InputTag);
	void Input_Custom_Triggered(const FInputActionValue& InputActionValue, FGameplayTag InputTag);
	void Input_Custom_Ongoing(const FInputActionValue& InputActionValue, FGameplayTag InputTag);
	void Input_Custom_Canceled(const FInputActionValue& InputActionValue, FGameplayTag InputTag);
	void Input_Custom_Completed(const FInputActionValue& InputActionValue, FGameplayTag InputTag);

	UPROPERTY(Transient)
	TArray<FInstancedStruct> CustomInputActions;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	FVector2D MoveInputVector = FVector2D::ZeroVector;

	bool bReadyToBindInputs = false;
	TArray<uint32> BindHandles;
};
