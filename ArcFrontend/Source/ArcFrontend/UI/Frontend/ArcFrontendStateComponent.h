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
#include "GameFeaturePluginOperationResult.h"
#include "LoadingProcessInterface.h"
#include "ControlFlowNode.h"

#include "CommonUserSubsystem.h"

#include "ArcFrontendStateComponent.generated.h"

enum class ECommonUserOnlineContext : uint8;
enum class ECommonUserPrivilege : uint8;
class UCommonActivatableWidget;
class UArcExperienceDefinition;
class UCommonUserInfo;

UCLASS(Abstract)
class UArcFrontendStateComponent : public UGameStateComponent, public ILoadingProcessInterface
{
	GENERATED_BODY()
private:
	bool bShouldShowLoadingScreen = true;
	FCommonUserInitializeParams LoginParams;
	/*
	 * If true will try to login to online subsystem and keep Loading screen for as long as we try to login.
	 */
	UPROPERTY(EditAnywhere, Category = UI)
		bool bLoginOnline = false;

	UPROPERTY(EditAnywhere, Category = UI)
		TSoftClassPtr<UCommonActivatableWidget> PressStartScreenClass;

	UPROPERTY(EditAnywhere, Category = UI)
		TSoftClassPtr<UCommonActivatableWidget> MainScreenClass;

	TSharedPtr<FControlFlow> FrontEndFlow;

	// If set, this is the in-progress press start screen task
	FControlFlowNodePtr InProgressPressStartScreen;
	FControlFlowNodePtr TryLoginFlow;
public:

	UArcFrontendStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	//~ILoadingProcessInterface interface
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	//~End of ILoadingProcessInterface

private:
	void OnExperienceLoaded(const UArcExperienceDefinition* Experience);

	UFUNCTION()
		void OnUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext);

	void FlowStep_WaitForUserInitialization(FControlFlowNodeRef SubFlow);
	void FlowStep_TryShowPressStartScreen(FControlFlowNodeRef SubFlow);
	void FlowStep_TryShowMainScreen(FControlFlowNodeRef SubFlow);

	void TryOnlineLogin(FControlFlowNodeRef SubFlow);

	UFUNCTION()
	void HandleLoginComplete(const UCommonUserInfo* UserInfo
		, bool bSuccess
		, FText Error
		, ECommonUserPrivilege RequestedPrivilege
		, ECommonUserOnlineContext OnlineContext);
};

