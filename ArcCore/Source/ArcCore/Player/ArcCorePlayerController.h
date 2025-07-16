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

#include "CommonPlayerController.h"
#include "ArcMacroDefines.h"
#include "GameplayTagAssetInterface.h"
#include "LoadingProcessInterface.h"

#include "Commands/ArcReplicatedCommand.h"

#include "ArcCorePlayerController.generated.h"

struct FArcReplicatedCommandHandle;
class UGameplayCameraSystemHost;
class IGameplayCameraSystemHost;

DECLARE_LOG_CATEGORY_EXTERN(LogArcCorePlayerController
	, Log
	, Log);

DECLARE_MULTICAST_DELEGATE_OneParam(FArcPlayerInputRotationDeletage
	, float);

UCLASS()
class ARCCORE_API AArcCorePlayerController
		: public ACommonPlayerController
		, public ILoadingProcessInterface
		, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

private:
	uint8 bComponentsInitialized:1 = false;
	/*
	 * How fast will Modifier be interpolated to new value.
	 */
	float PitchYawModifierInterpolation = 20.0f;
	/*
	 * Add to RotationInput.Pitch before UpdateRotation. The value is accumulated and
	 * interpolated each tick before RotationInput is added to Camera/Control Rotation.
	 */
	float PitchModifier = 0;
	float OldPitchModifier = 0;
	/*
	 * Add to RotationInput.Yaw before UpdateRotation. The value is accumulated and
	 * interpolated each tick before RotationInput is added to Camera/Control Rotation.
	 */
	float YawModifier = 0;
	float OldYawModifier = 0;
	float LastCombinedRotation = 0;
	float CombinedRotation = 0;
	
public:
	FRotator ControlRotationOffset = FRotator::ZeroRotator;

	FVector2D InputValue;
	TOptional<float> ClampValue;
	
	DEFINE_ARC_DELEGATE(FArcPlayerInputRotationDeletage, OnPreProcessRotation);
	DEFINE_ARC_DELEGATE(FArcPlayerInputRotationDeletage, OnRotationInputAdd);

public:
	IGameplayCameraSystemHost* GameplayCameraSystemHost = nullptr;
	

	IGameplayCameraSystemHost* GetGameplayCameraSystemHost();

	//~ILoadingProcessInterface interface
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;

	//~End of ILoadingProcessInterface

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	void SetPitchYawModifierInterpolation(float Value)
	{
		PitchYawModifierInterpolation = Value;
	}

	void AddPitchModifier(float Value)
	{
		OldPitchModifier = PitchModifier;
		PitchModifier = Value;
	}

	void AddYawModifier(float Value)
	{
		OldYawModifier = YawModifier;
		YawModifier = Value;
	}

	void ResetPitchModifier()
	{
		OldPitchModifier = 0;
		PitchModifier = 0;
	}

	void ResetYawModifier()
	{
		OldYawModifier = 0;
		YawModifier = 0;
	}

	float GetCombinedRotation() const
	{
		return CombinedRotation;
	}

	float GetLastCombinedRotation() const
	{
		return LastCombinedRotation;
	}

public:
	// Sets default values for this actor's properties
	AArcCorePlayerController();

	virtual void PreInitializeComponents() override;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction ) override;
	
	virtual void PreProcessInput(const float DeltaTime
								 , const bool bGamePaused) override;

	virtual void PostProcessInput(const float DeltaTime
								  , const bool bGamePaused) override;

public:
	virtual void OnRep_Pawn() override; ;

	virtual void OnPossess(APawn* InPawn) override;
	
	virtual void OnRep_PlayerState() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void InitPlayerState() override;

	virtual void AddPitchInput(float Val) override;

	virtual void AddYawInput(float Val) override;

	UFUNCTION(Exec)
	void ExecuteCommand();

	UFUNCTION(Server, Reliable)
	void ServerExecuteCommand();
	
	UFUNCTION(Exec)
	void OpenMap(const FString& MapName);

	UFUNCTION(Server, Reliable)
	void ServerOpenMap(const FString& MapName);

private:
	TMap<FArcReplicatedCommandId, FArcCommandConfirmedDelegate> CommandResponses;
	
public:
	bool SendReplicatedCommand(const FArcReplicatedCommandHandle& Command);

	bool SendReplicatedCommand(const FArcReplicatedCommandHandle& Command, FArcCommandConfirmedDelegate&& ConfirmDelegate);
	
protected:
	UFUNCTION(Server, Reliable)
	void ServerSendReplicatedCommand(const FArcReplicatedCommandHandle& Command);

	UFUNCTION(Client, Reliable)
	void ClientConfirmReplicatedCommand(const FArcReplicatedCommandId& CommandId, bool bResult);
	
public:
	bool SendClientReplicatedCommand(const FArcReplicatedCommandHandle& Command);

protected:
	UFUNCTION(Client, Reliable)
	void ClientSendReplicatedCommand(const FArcReplicatedCommandHandle& Command);
};

UCLASS()
class ARCCORE_API UArcCorePlayerControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	virtual void OnPossesed(APawn* InPawn) {};
};
