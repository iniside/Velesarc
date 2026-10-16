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

#include "CommonPlayerController.h"
#include "ArcMacroDefines.h"

#include "Commands/ArcReplicatedCommand.h"
#include "Commands/ArcReplicatedCommandDispatch.h"
#include "Commands/ArcReplicatedCommandHost.h"

#include "ArcMassPlayerController.generated.h"

struct FArcReplicatedCommandHandle;
class IGameplayCameraSystemHost;
class UArcMassPlayerControllerExtensionComponent;
class UAsyncMessageBindingComponent;
class UGameplayControlRotationComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogArcMassPlayerController, Log, Log);

DECLARE_MULTICAST_DELEGATE_OneParam(FArcMassPlayerInputRotationDelegate, float);

UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API AArcMassPlayerController
		: public ACommonPlayerController
		, public IArcReplicatedCommandHost
{
	GENERATED_BODY()

public:
	DEFINE_ARC_DELEGATE(FArcMassPlayerInputRotationDelegate, OnPitchInputAdd);
	DEFINE_ARC_DELEGATE(FArcMassPlayerInputRotationDelegate, OnYawInputAdd);

	IGameplayCameraSystemHost* GameplayCameraSystemHost = nullptr;

	IGameplayCameraSystemHost* GetGameplayCameraSystemHost();

	//
	// Recoil API (lifted from AArcCorePlayerController in 4h Phase 2a).
	// Consumed by ArcGunRecoilInstance (writes ControlRotationOffset) and the
	// ArcCore camera nodes (reads ControlRotationOffset / InputValue / ClampValue).
	//
public:
	/** Accumulated camera-rotation offset injected by recoil; consumed and zeroed by the camera input node each tick. */
	FRotator ControlRotationOffset = FRotator::ZeroRotator;

	/** Last camera input value; published by the camera input node, read by gameplay. */
	FVector2D InputValue = FVector2D::ZeroVector;

	/** Optional clamp on the Y component of camera input (sign chooses min/max half). */
	TOptional<float> ClampValue;

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

private:
	float PitchYawModifierInterpolation = 20.0f;
	float PitchModifier = 0;
	float OldPitchModifier = 0;
	float YawModifier = 0;
	float OldYawModifier = 0;
	float LastCombinedRotation = 0;
	float CombinedRotation = 0;

private:
	UPROPERTY(VisibleAnywhere, Category = "ArcMass|PlayerController")
	TObjectPtr<UArcMassPlayerControllerExtensionComponent> MyPlayerControllerExtensionComponent;

	/** Owns this controller's per-player async-message binding endpoint; lets messages (e.g. interaction
	 *  acquire/invalidate) be scoped to this local player instead of broadcast world-wide. */
	UPROPERTY(VisibleAnywhere, Category = "ArcMass|PlayerController")
	TObjectPtr<UAsyncMessageBindingComponent> AsyncMessageBindingComponent;

	// Cached during PreInitializeComponents/BeginPlay; manually ticked from TickActor
	// after Super::TickActor (which runs UpdateRotation) so camera-driven rotation
	// applies in the right order. The component's own tick is disabled on cache.
	UPROPERTY(Transient)
	TObjectPtr<UGameplayControlRotationComponent> ControlRotationComp = nullptr;

public:
	AArcMassPlayerController();

	virtual void PreInitializeComponents() override;

protected:
	virtual void BeginPlay() override;

	virtual void TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

private:
	void CacheControlRotationComp();

protected:

public:
	virtual void AddPitchInput(float Val) override;

	virtual void AddYawInput(float Val) override;

private:
	TMap<FGuid, FArcCommandConfirmedDelegate> CommandResponses;

	TArray<FArcPredictedCommand> PredictedCommands;

	uint32 PredictionSequence = 0;

	bool bPredictionReplaying = false;

	bool ShouldPredict(const FArcReplicatedCommandHandle& Command) const;

public:
	virtual bool SendReplicatedCommand(const FArcReplicatedCommandHandle& Command) override;

	virtual bool SendReplicatedCommand(const FArcReplicatedCommandHandle& Command, FArcCommandConfirmedDelegate&& ConfirmDelegate) override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSendReplicatedCommand(const FArcReplicatedCommandHandle& Command);

	UFUNCTION(Client, Reliable)
	void ClientConfirmReplicatedCommand(const FGuid& CommandId, bool bResult);

public:
	virtual bool SendClientReplicatedCommand(const FArcReplicatedCommandHandle& Command) override;

protected:
	UFUNCTION(Client, Reliable)
	void ClientSendReplicatedCommand(const FArcReplicatedCommandHandle& Command);
};
