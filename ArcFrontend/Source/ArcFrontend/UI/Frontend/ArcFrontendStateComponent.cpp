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

#include "UI/Frontend/ArcFrontendStateComponent.h"

#include "ArcCoreGameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFeaturesSubsystem.h"
#include "Core/ArcCoreAssetManager.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameMode/ArcExperienceManager.h"
#include "GameMode/ArcExperienceManagerComponent.h"
#include "GameMode/ArcExperienceDefinition.h"
#include "TimerManager.h"
#include "NativeGameplayTags.h"
#include "ControlFlowManager.h"
#include "CommonUIExtensions.h"
#include "Kismet/GameplayStatics.h"
#include "PrimaryGameLayout.h"
#include "ICommonUIModule.h"
#include "CommonUISettings.h"
#include "CommonUserSubsystem.h"
#include "CommonSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Online/Auth.h"
#include "Online/Lobbies.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineServices.h"
#include "Online/OnlineServicesEngineUtils.h"
#include "Online/OnlineServicesRegistry.h"

namespace FrontendState
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PLATFORM_TRAIT_SINGLEONLINEUSER, "Platform.Trait.SingleOnlineUser");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_LAYER_MENU, "UI.Layer.Menu");
}

UArcFrontendStateComponent::UArcFrontendStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcFrontendStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// Listen for the experience load to complete
	AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
	UArcExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcExperienceManagerComponent>();
	check(ExperienceComponent);
	ExperienceComponent->CallOrRegister_OnExperienceLoaded_HighPriority(FOnArcExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
	
}

void UArcFrontendStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

bool UArcFrontendStateComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (bShouldShowLoadingScreen)
	{
		OutReason = TEXT("Frontend Flow Pending...");
		
		if (FrontEndFlow.IsValid())
		{
			const TOptional<FString> StepDebugName = FrontEndFlow->GetCurrentStepDebugName();
			if (StepDebugName.IsSet())
			{
				OutReason = StepDebugName.GetValue();
			}
		}
		
		return true;
	}

	return false;
}

void UArcFrontendStateComponent::OnExperienceLoaded(const UArcExperienceDefinition* Experience)
{
	FControlFlow& Flow = FControlFlowStatics::Create(this, TEXT("FrontendFlow"))
		//.QueueStep(TEXT("Wait For User Initialization"), this, &ThisClass::FlowStep_WaitForUserInitialization)
		.QueueStep(TEXT("Try Online Login"), this, &ThisClass::TryOnlineLogin)
		//.QueueStep(TEXT("Try Show Press Start Screen"), this, &ThisClass::FlowStep_TryShowPressStartScreen)
		.QueueStep(TEXT("Try Show Main Screen"), this, &ThisClass::FlowStep_TryShowMainScreen);

	Flow.ExecuteFlow();

	FrontEndFlow = Flow.AsShared();
}

void UArcFrontendStateComponent::FlowStep_WaitForUserInitialization(FControlFlowNodeRef SubFlow)
{
	// If this was a hard disconnect, explicitly destroy all user and session state
	// TODO: Refactor the engine disconnect flow so it is more explicit about why it happened
	bool bWasHardDisconnect = false;
	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AGameModeBase>();
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	ENetMode NM = GetNetMode();
	if(NM != ENetMode::NM_Client)
	{
		if (ensure(GameMode) && UGameplayStatics::HasOption(GameMode->OptionsString, TEXT("closed")))
		{
			bWasHardDisconnect = true;
		}
	}
	

	// Only reset users on hard disconnect
	UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();
	if (ensure(UserSubsystem) && bWasHardDisconnect)
	{
		UserSubsystem->ResetUserState();
	}
	
	// Always reset sessions
	UCommonSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UCommonSessionSubsystem>();
	if (ensure(SessionSubsystem))
	{
		SessionSubsystem->CleanUpSessions();
	}

	SubFlow->ContinueFlow();
}

void UArcFrontendStateComponent::FlowStep_TryShowPressStartScreen(FControlFlowNodeRef SubFlow)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();

	// Check to see if the first player is already logged in, if they are, we can skip the press start screen.
	//if (const UCommonUserInfo* FirstUser = UserSubsystem->GetUserInfoForLocalPlayerIndex(0))
	//{
	//	if (FirstUser->InitializationState == ECommonUserInitializationState::LoggedInLocalOnly ||
	//		FirstUser->InitializationState == ECommonUserInitializationState::LoggedInOnline)
	//	{
	//		SubFlow->ContinueFlow();
	//		return;
	//	}
	//}

	// Check to see if the platform actually requires a 'Press Start' screen.  This is only
	// required on platforms where there can be multiple online users where depending on what player's
	// controller presses 'Start' establishes the player to actually login to the game with.
	if (!UserSubsystem->ShouldWaitForStartInput())
	{
		// Start the auto login process, this should finish quickly
		FInputDeviceId DeviceId;
		InProgressPressStartScreen = SubFlow;
		UserSubsystem->OnUserInitializeComplete.AddDynamic(this, &UArcFrontendStateComponent::OnUserInitialized);
		UserSubsystem->TryToInitializeForLocalPlay(0, DeviceId, false);
		//UserSubsystem->TryToLoginForOnlinePlay(0);
		//return;
	}
	SubFlow->ContinueFlow();
	
	// Add the Press Start screen, move to the next flow when it deactivates.
	//if (UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayoutForPrimaryPlayer(this))
	//{
	//	const bool bSuspendInputUntilComplete = true;
	//	RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(FrontendState::TAG_UI_LAYER_MENU, bSuspendInputUntilComplete, PressStartScreenClass, [this, SubFlow](EAsyncWidgetLayerState State, UCommonActivatableWidget* Screen) {
	//		switch (State)
	//		{
	//		case EAsyncWidgetLayerState::AfterPush:
	//			bShouldShowLoadingScreen = false;
	//			Screen->OnDeactivated().AddWeakLambda(this, [this, SubFlow]() {
	//				SubFlow->ContinueFlow();
	//			});
	//			break;
	//		case EAsyncWidgetLayerState::Canceled:
	//			bShouldShowLoadingScreen = false;
	//			SubFlow->ContinueFlow();
	//			return;
	//		}
	//	});
	//}
}

void UArcFrontendStateComponent::OnUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext)
{
	FControlFlowNodePtr FlowToContinue = InProgressPressStartScreen;
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();

	if (ensure(FlowToContinue.IsValid() && UserSubsystem))
	{
		UserSubsystem->OnUserInitializeComplete.RemoveDynamic(this, &UArcFrontendStateComponent::OnUserInitialized);
		InProgressPressStartScreen.Reset();
	
		if (bSuccess)
		{
			// On success continue flow normally
			FlowToContinue->ContinueFlow();
		}
		else
		{
			// TODO: Just continue for now, could go to some sort of error screen
			FlowToContinue->ContinueFlow();
		}
	}

	FlowToContinue->ContinueFlow();
}

void UArcFrontendStateComponent::FlowStep_TryShowMainScreen(FControlFlowNodeRef SubFlow)
{
	UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayoutForPrimaryPlayer(this);
	if (RootLayout)
	{
		const bool bSuspendInputUntilComplete = true;
		RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(FrontendState::TAG_UI_LAYER_MENU, bSuspendInputUntilComplete, MainScreenClass, [this, SubFlow](EAsyncWidgetLayerState State, UCommonActivatableWidget* Screen) {
			switch (State)
			{
			case EAsyncWidgetLayerState::AfterPush:
				Screen->ActivateWidget();
				bShouldShowLoadingScreen = false;
				SubFlow->ContinueFlow();
				return;
			case EAsyncWidgetLayerState::Canceled:
				bShouldShowLoadingScreen = false;
				SubFlow->ContinueFlow();
				return;
			}
		});
	}
	else
	{
		FTimerManager& TimerManager = GetWorld()->GetTimerManager();
		TimerManager.SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UArcFrontendStateComponent::FlowStep_TryShowMainScreen, SubFlow));
	}
}

void UArcFrontendStateComponent::TryOnlineLogin(FControlFlowNodeRef SubFlow)
{
#if WITH_EDITOR
	UE::Online::GetServicesEngineUtils()->SetShouldTryOnlinePIE(true);
#endif

	UE::Online::FOnlineServicesRegistry& Registry = UE::Online::FOnlineServicesRegistry::Get();
	UE::Online::IOnlineServicesPtr OnlineServices = UE::Online::GetServices(UE::Online::EOnlineServices::Epic);
	
	UE::Online::IAuthPtr AuthPtr = OnlineServices->GetAuthInterface();

	UE::Online::FAuthLogin::Params LoginParamsv2;
	LoginParamsv2.PlatformUserId = FPlatformUserId::CreateFromInternalId(0);
	LoginParamsv2.CredentialsId = "localhost:5555";
	LoginParamsv2.CredentialsToken.Emplace<FString>(TEXT("iniside"));
	LoginParamsv2.CredentialsType = UE::Online::LoginCredentialsType::Developer;

	UE::Online::FAuthGetAllLocalOnlineUsers::Params UserParams;
	UE::Online::FAuthGetAllLocalOnlineUsers::Result DefaultResult;
	UE::Online::FAuthGetAllLocalOnlineUsers::Result Res = AuthPtr->GetAllLocalOnlineUsers(MoveTemp(UserParams)).GetOkOrDefault(DefaultResult);
	if (Res.AccountInfo.Num() > 0)
	{
		if (AuthPtr->IsLoggedIn(Res.AccountInfo[0]->AccountId))
		{
			UE::Online::ILobbiesPtr LobbyPtr = OnlineServices->GetLobbiesInterface();

			UE::Online::FCreateLobby::Params LobbyParams;
			LobbyParams.LocalAccountId = Res.AccountInfo[0]->AccountId;
			LobbyParams.LocalName = "Super iniside lobby";
			LobbyParams.SchemaId = "GameLobby";
			LobbyParams.bPresenceEnabled = true;
			LobbyParams.MaxMembers = 4;
			LobbyParams.JoinPolicy = UE::Online::ELobbyJoinPolicy::PublicAdvertised;
				
			LobbyPtr->CreateLobby(MoveTemp(LobbyParams)).OnComplete(this, [SubFlow](const UE::Online::TOnlineResult<UE::Online::FCreateLobby>& LobbyResult)
			{
				if (LobbyResult.IsOk())
				{
					UE_LOG(LogTemp, Log, TEXT("Lobby success"));
					//SubFlow->ContinueFlow();
						
				}
			});
			SubFlow->ContinueFlow();
		}
	}
	
	AuthPtr->Login(MoveTemp(LoginParamsv2)).OnComplete(this, [this, SubFlow,OnlineServices](const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& Result)
		{
			if (Result.IsOk())
			{
				UE::Online::FAuthQueryExternalServerAuthTicket::Params AuthTokenParams;
				AuthTokenParams.LocalAccountId = Result.GetOkValue().AccountInfo->AccountId;
				
				UE::Online::IAuthPtr AuthPtr2 = OnlineServices->GetAuthInterface();
				AuthPtr2->QueryExternalServerAuthTicket(MoveTemp(AuthTokenParams)).OnComplete(this, [this, OnlineServices](const UE::Online::TOnlineResult<UE::Online::FAuthQueryExternalServerAuthTicket>& Result)
				{
					if (Result.IsError())
					{
						return;
					}

					UE::Online::FExternalServerAuthTicket val = Result.GetOkValue().ExternalServerAuthTicket;
				});
				
				FString StringId;
				StringId = UE::Online::FOnlineIdRegistryRegistry::Get().ToString(Result.GetOkValue().AccountInfo->AccountId);

				TArray<uint8> bytes = UE::Online::FOnlineIdRegistryRegistry::Get().ToReplicationData(Result.GetOkValue().AccountInfo->AccountId);
				UE::Online::FAccountId newId = UE::Online::FOnlineIdRegistryRegistry::Get().ToAccountId(UE::Online::EOnlineServices::Epic, bytes);

				FString StringId2;
				StringId2 = UE::Online::FOnlineIdRegistryRegistry::Get().ToString(newId);
				
				UArcCoreGameInstance* ArcGI = GetWorld()->GetGameInstance<UArcCoreGameInstance>();
				FUniqueNetIdRepl NetId;
				NetId.SetAccountId(Result.GetOkValue().AccountInfo->AccountId);
				ArcGI->SetMyNetId(NetId);
				
				UE::Online::ILobbiesPtr LobbyPtr = OnlineServices->GetLobbiesInterface();

				UE::Online::FCreateLobby::Params LobbyParams;
				LobbyParams.LocalAccountId = Result.GetOkValue().AccountInfo->AccountId;
				LobbyParams.LocalName = "Super iniside lobby";
				LobbyParams.SchemaId = "GameLobby";
				LobbyParams.bPresenceEnabled = true;
				LobbyParams.MaxMembers = 4;
				LobbyParams.JoinPolicy = UE::Online::ELobbyJoinPolicy::PublicAdvertised;
				
				LobbyPtr->CreateLobby(MoveTemp(LobbyParams)).OnComplete(this, [SubFlow](const UE::Online::TOnlineResult<UE::Online::FCreateLobby>& LobbyResult)
				{
					if (LobbyResult.IsOk())
					{
						UE_LOG(LogTemp, Log, TEXT("Lobby success"));
						SubFlow->ContinueFlow();
						
					}
				});
				
				
				UE_LOG(LogTemp, Log, TEXT("Login Success"));
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("Login Failed"));
			}
		});
}

void UArcFrontendStateComponent::HandleLoginComplete(const UCommonUserInfo* UserInfo
	, bool bSuccess
	, FText Error
	, ECommonUserPrivilege RequestedPrivilege
	, ECommonUserOnlineContext OnlineContext)
{
	if(TryLoginFlow.IsValid())
	{
		TryLoginFlow->ContinueFlow();
	}
	bShouldShowLoadingScreen = false;
}
