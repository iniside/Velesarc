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

#include "Player/ArcCorePlayerState.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Player/ArcCorePlayerController.h"
#include "Net/UnrealNetwork.h"

#include "Pawn/ArcPawnData.h"
#include "Components/GameFrameworkComponentManager.h"
// #include "ArcCore/AbilitySystem/ArcGlobalAbilitySystem.h"
#include "GameFramework/GameplayMessageSubsystem.h"

//@TODO: Would like to isolate this a bit better to get the pawn data in here without this
// having to know about other
// stuff
#include "ArcCoreGameInstance.h"
#include "ArcCoreUtils.h"
#include "ArcPlayerStateExtensionComponent.h"
#include "ArcWorldDelegates.h"
#include "GameMode/ArcCoreGameMode.h"
#include "GameMode/ArcExperienceManagerComponent.h"
#include "Engine/AssetManager.h"
#include "GameMode/ArcExperienceDefinition.h"

#include "GameMode/ArcCoreWorldSettings.h"
#include "Online/Auth.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineServices.h"
#include "Online/OnlineServicesRegistry.h"
#include "Online/UserFile.h"
#include "Pawn/ArcPawnExtensionComponent.h"

DEFINE_LOG_CATEGORY(LogArcPlayerState);

const FName AArcCorePlayerState::NAME_ArcAbilityReady("ArcAbilitiesReady");

AArcCorePlayerState::AArcCorePlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MyPlayerConnectionType(EArcPlayerConnectionType::Player)
{
	// TODO: Irrelevelant now I guess ?
	SetNetUpdateFrequency(30.0f);
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	MyTeamId = INDEX_NONE;
	MySquadId = INDEX_NONE;
}

void AArcCorePlayerState::PreRegisterAllComponents()
{
	if (Arcx::Utils::IsPlayableWorld(GetWorld()))
	{
		UArcWorldDelegates* WorldDelegates = UArcWorldDelegates::Get(this);
		if (WorldDelegates)
		{
			WorldDelegates->BroadcastClassOnActorPreRegisterAllComponents(GetClass(), this);
		}		
	}
	
	
	Super::PreRegisterAllComponents();
}

void AArcCorePlayerState::OnSetUniqueId()
{
	UArcWorldDelegates* WorldDelegates = UArcWorldDelegates::Get(this);
	
	if (WorldDelegates && GetNetMode() == NM_Standalone)
	{
		WorldDelegates->BroadcastOnUniqueNetIdSet(this, GetUniqueId());
	}
}

void AArcCorePlayerState::OnRep_UniqueId()
{
	Super::OnRep_UniqueId();

	if (GetNetMode() != NM_Client)
	{
		return;
	}

	UE::Online::FOnlineServicesRegistry& Registry = UE::Online::FOnlineServicesRegistry::Get();
	UE::Online::IOnlineServicesPtr OnlineServices = UE::Online::GetServices(UE::Online::EOnlineServices::Epic);
	
	UE::Online::IAuthPtr AuthPtr = OnlineServices->GetAuthInterface();
	UE::Online::FAuthQueryExternalServerAuthTicket::Params AuthTokenParams;
	AuthTokenParams.LocalAccountId = GetUniqueId().GetV2();
	AuthPtr->QueryExternalServerAuthTicket(MoveTemp(AuthTokenParams)).OnComplete(this, [this](const UE::Online::TOnlineResult<UE::Online::FAuthQueryExternalServerAuthTicket>& Result)
	{
		if (Result.IsError())
		{
			return;
		}
		UE::Online::FExternalServerAuthTicket val = Result.GetOkValue().ExternalServerAuthTicket;
		ServerSendTokenAuth(val.Type, val.Data);
	});
}

void AArcCorePlayerState::ServerSendTokenAuth_Implementation(const FName& Type
	, const FString& Token)
{
	UE::Online::FOnlineServicesRegistry& Registry = UE::Online::FOnlineServicesRegistry::Get();
	UE::Online::IOnlineServicesPtr OnlineServices = UE::Online::GetServices(UE::Online::EOnlineServices::Epic);


	UE::Online::IUserFilePtr UserFile = OnlineServices->GetUserFileInterface();

	//AuthPtr->QueryExternalServerAuthTicket()
	FString Path = "Path/File.txe";
	FString FileContent = "Some Content";
		
	UE::Online::FUserFileWriteFile::Params WriteParams;
	WriteParams.Filename = Path;
	WriteParams.LocalAccountId = GetUniqueId().GetV2();
		
	FTCHARToUTF8 Converter(*FileContent);
	TArray<uint8> ByteArray;
	ByteArray.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	WriteParams.FileContents = ByteArray;
			
	UserFile->WriteFile(MoveTemp(WriteParams)).OnComplete(this, [](const UE::Online::TOnlineResult<UE::Online::FUserFileWriteFile>& FileResult)
	{
		if (FileResult.IsOk())
		{
			UE_LOG(LogTemp, Log, TEXT("Write Success"));
		}
	});
	
	//UE::Online::IAuthPtr AuthPtr = OnlineServices->GetAuthInterface();
//
	//UE::Online::FAuthLogin::Params LoginParamsv3;
	//LoginParamsv3.PlatformUserId = FPlatformUserId::CreateFromInternalId(GetPlayerId());
	////LoginParamsv3.CredentialsId =
	//UE::Online::FExternalAuthToken Tok;
	//Tok.Type = Type;
	//Tok.Data = Token;
	//LoginParamsv3.CredentialsToken.Emplace<UE::Online::FExternalAuthToken>(Tok);
	//LoginParamsv3.CredentialsType = UE::Online::LoginCredentialsType::ExternalAuth;
	//LoginParamsv3.Scopes.Add("BasicProfile");
	////LoginParamsv3.CredentialsType = UE::Online::LoginCredentialsType::ExchangeCode;
//
	//UE::Online::FAuthQueryVerifiedAuthTicket::Params AuthTokenParams;
	//AuthTokenParams.LocalAccountId = GetUniqueId().GetV2();
	//AuthPtr->QueryVerifiedAuthTicket(MoveTemp(AuthTokenParams)).OnComplete(
	//	this, [this, OnlineServices, AuthPtr](const UE::Online::TOnlineResult<UE::Online::FAuthQueryVerifiedAuthTicket>& Result)
	//{
	//	if (Result.IsError())
	//	{
	//		return;
	//	}
//
	//	UE::Online::FVerifiedAuthTicket val = Result.GetOkValue().VerifiedAuthTicket;
	//});
}

void AArcCorePlayerState::PreInitializeComponents()
{
	UArcPlayerStateExtensionComponent* PSE = NewObject<UArcPlayerStateExtensionComponent>(this
		, UArcPlayerStateExtensionComponent::StaticClass()
		, "PlayerStateExtension");
	PSE->RegisterComponentWithWorld(GetWorld());
	PSE->SetIsReplicated(true);
	PSE->SetNetAddressable();

	Super::PreInitializeComponents();
}

void AArcCorePlayerState::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogArcPlayerState, Log, TEXT("AArcCorePlayerState::BeginPlay CheckDataReady [%s]!"), *GetNameSafe(this));
	
	CheckDataReady();

	
	UArcWorldDelegates::Get(this)->BroadcastOnPlayerStateBeginPlay(this);
	if (GetNetMode() == NM_Client)
	{
		UArcCoreGameInstance* ArcGI = GetWorld()->GetGameInstance<UArcCoreGameInstance>();
		const FUniqueNetIdRepl& MyId = ArcGI->GetMyNetId();
		if (MyId.IsV2())
		{
			ServerSetUniqueNetId(MyId);
		}
	}

	
#if WITH_EDITOR
	//UE::Online::GetServicesEngineUtils()->SetShouldTryOnlinePIE(true);
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
			FUniqueNetIdRepl New(Res.AccountInfo[0]->AccountId);
			SetUniqueId(New);
			return;
		}
	}
	
	AuthPtr->Login(MoveTemp(LoginParamsv2)).OnComplete(this, [this, OnlineServices](const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& Result)
	{
		if (Result.IsOk())
		{
			FUniqueNetIdRepl New( Result.GetOkValue().AccountInfo->AccountId);
			SetUniqueId(New);
		}
	});
}

void AArcCorePlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// if (UArcGlobalAbilitySystem* GlobalAbilitySystem =
	// UWorld::GetSubsystem<UArcGlobalAbilitySystem>(GetWorld()))
	//{
	//	GlobalAbilitySystem->UnregisterASC(AbilitySystemComponent);
	// }

	Super::EndPlay(EndPlayReason);
}

void AArcCorePlayerState::ServerSetUniqueNetId_Implementation(const FUniqueNetIdRepl& NetId)
{
	UArcWorldDelegates* WorldDelegates = UArcWorldDelegates::Get(this);
	if (WorldDelegates)
	{
		WorldDelegates->BroadcastOnUniqueNetIdSet(this, NetId);
	}
}

void AArcCorePlayerState::PostNetInit()
{
	Super::PostNetInit();
}

void AArcCorePlayerState::OnReplicationStartedForIris(const FOnReplicationStartedParams& Params)
{
	Super::OnReplicationStartedForIris(Params);
}

void AArcCorePlayerState::ClientInitialize(AController* C)
{
	Super::ClientInitialize(C);

	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(GetPawn()))
	{
		PawnExtComp->CheckDefaultInitialization();
	}
}

void AArcCorePlayerState::Reset()
{
	Super::Reset();
}

void AArcCorePlayerState::CopyProperties(APlayerState* PlayerState)
{
	// TODO: Seamless Travel. for now disable it. 
	// Super::CopyProperties(PlayerState);
}

void AArcCorePlayerState::OnDeactivated()
{
	bool bDestroyDeactivatedPlayerState = false;

	switch (GetPlayerConnectionType())
	{
		case EArcPlayerConnectionType::Player:
		case EArcPlayerConnectionType::InactivePlayer:
			//@TODO: Ask the experience if we should destroy disconnecting players
			// immediately
			// (e.g., for long running servers where they might build up if lots of
			// players cycle through)
			bDestroyDeactivatedPlayerState = false;
			break;
		default:
			bDestroyDeactivatedPlayerState = true;
			break;
	}

	SetPlayerConnectionType(EArcPlayerConnectionType::InactivePlayer);

	if (bDestroyDeactivatedPlayerState)
	{
		Destroy();
	}
}

void AArcCorePlayerState::OnReactivated()
{
	if (GetPlayerConnectionType() == EArcPlayerConnectionType::InactivePlayer)
	{
		SetPlayerConnectionType(EArcPlayerConnectionType::Player);
	}
}

void AArcCorePlayerState::OnExperienceLoaded(const UArcExperienceDefinition* CurrentExperience)
{
	const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	
	if (CurrentExperience->GetDefaultPawnData().IsValid())
	{
		if (const UArcPawnData* NewPawnData = CurrentExperience->GetDefaultPawnData().Get())
		{
			SetPawnData(NewPawnData);
		}
	}
	else
	{
		UE_LOG(LogArcPlayerState, Error, TEXT("AArcCorePlayerState::OnExperienceLoaded(): Unable to find PawnData to initialize player state [%s]!"), *GetNameSafe(this));
	}
}

void AArcCorePlayerState::CheckDataReady(const UArcPawnData* InPawnData)
{
	if (PawnData == nullptr && InPawnData != nullptr)
	{
		UE_LOG(LogArcPlayerState, Log, TEXT("AArcCorePlayerState::CheckDataReady Set PawnData from Parameter"))
		PawnData = const_cast<UArcPawnData*>(InPawnData);
	}
	
	UE_LOG(LogArcPlayerState, Log, TEXT("AArcCorePlayerState::CheckDataReady [Pawn %s] -- [%s] -- [PawnData %s]")
			, *GetNameSafe(GetPawn())
			, *GetNameSafe(this)
			, *GetNameSafe(PawnData));
	
	if(PawnData != nullptr && bPawnDataInitialized == false && GetPawn())
	{
		UE_LOG(LogArcPlayerState, Log, TEXT("AArcCorePlayerState::CheckDataReady Call Data Ready [%s]!"), *GetNameSafe(this));
		
		bPawnDataInitialized = true;

		if (UArcPawnExtensionComponent* PawnExtension = UArcPawnExtensionComponent::FindPawnExtensionComponent(GetPawn()))
		{
			PawnExtension->HandlePawnDataReplicated();
		}

		for (TComponentIterator<UArcCorePlayerStateComponent> It(this); It; ++It)
		{
			It->OnPawnDataReady(GetPawn());
		}
	}
}

void AArcCorePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.RepNotifyCondition = ELifetimeRepNotifyCondition::REPNOTIFY_Always;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, PawnData
		, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, MyPlayerConnectionType
		, SharedParams)

	// TODO: Actually remove it and try to make separate plugin for it.
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, MyTeamId
		, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, MySquadId
		, SharedParams);

	// DOREPLIFETIME(ThisClass, StatTags);
}

AArcCorePlayerController* AArcCorePlayerState::GetArcPlayerController() const
{
	return CastChecked<AArcCorePlayerController>(GetOwner()
		, ECastCheckedType::NullAllowed);
}

UArcCoreAbilitySystemComponent* AArcCorePlayerState::GetArcAbilitySystemComponent() const
{
	if (!AbilitySystemComponent)
	{
		AbilitySystemComponent = FindComponentByClass<UArcCoreAbilitySystemComponent>();
	}
	return AbilitySystemComponent;
}

void AArcCorePlayerState::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (GetArcAbilitySystemComponent())
	{
		GetArcAbilitySystemComponent()->GetOwnedGameplayTags(TagContainer);
	}
}

void AArcCorePlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (UArcCoreAbilitySystemComponent* ASC = FindComponentByClass<UArcCoreAbilitySystemComponent>())
	{
		ASC->InitAbilityActorInfo(this
			, GetPawn());
	}

	AGameStateBase* GameState = GetWorld()->GetGameState();
	if (GetNetMode() != NM_Client && GameState)
	{
		check(GameState);

		UArcExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcExperienceManagerComponent>();
		check(ExperienceComponent);
		
		ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnArcExperienceLoaded::FDelegate::CreateUObject(this
			, &ThisClass::OnExperienceLoaded));
	}
}

void AArcCorePlayerState::SetPawnData(const UArcPawnData* InPawnData)
{
	check(InPawnData);

	if (PawnData)
	{
		UE_LOG(LogArcPlayerState
			, Log
			, TEXT( "Trying to set PawnData [%s] on player state [%s] that already has valid PawnData [%s].")
			, *GetNameSafe(InPawnData)
			, *GetNameSafe(this)
			, *GetNameSafe(PawnData));
		return;
	}

	PawnData = const_cast<UArcPawnData*>(InPawnData);

	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass
		, PawnData
		, this);

	if (UArcCoreAbilitySystemComponent* ASC = FindComponentByClass<UArcCoreAbilitySystemComponent>())
	{
		UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this
			, NAME_ArcAbilityReady);
	}
	
	// if (UArcGlobalAbilitySystem* GlobalAbilitySystem =
	// UWorld::GetSubsystem<UArcGlobalAbilitySystem>(GetWorld()))
	//{
	//	GlobalAbilitySystem->RegisterASC(AbilitySystemComponent);
	// }

	UE_LOG(LogArcPlayerState, Log, TEXT("AArcCorePlayerState::SetPawnData CheckDataReady [%s]!"), *GetNameSafe(this));
	CheckDataReady(InPawnData);

	//@TODO: Still needed with push model?
	ForceNetUpdate();
}

void AArcCorePlayerState::OnRep_PawnData()
{
	UE_LOG(LogArcPlayerState, Log, TEXT("AArcCorePlayerState::OnRep_PawnData CheckDataReady [%s]!"), *GetNameSafe(this));
	CheckDataReady(PawnData);
}

void AArcCorePlayerState::SetPlayerConnectionType(EArcPlayerConnectionType NewType)
{
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass
		, MyPlayerConnectionType
		, this);
	MyPlayerConnectionType = NewType;
}

void AArcCorePlayerState::SetSquadId(int32 NewSquadId)
{
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass
			, MySquadId
			, this);

		MySquadId = NewSquadId;
	}
}

void AArcCorePlayerState::SetTeamId(int32 NewTeamId)
{
	if (HasAuthority())
	{
		const int32 OldTeamId = MyTeamId;
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass
			, MyTeamId
			, this);
		
		MyTeamId = NewTeamId;
		UE_LOG(LogArcPlayerState, Log, TEXT("[Server] ArcPS %s assigned team %d"), *GetUniqueId().ToDebugString(), MyTeamId);
		
		OnTeamChangedDelegate.Broadcast(OldTeamId, MyTeamId);
	}
}

void AArcCorePlayerState::OnRep_SquadId()
{
	//@TODO: Let the squad subsystem know
}

void AArcCorePlayerState::OnRep_TeamId(int32 OldTeamId)
{
	//@TODO: Let the team subsystem know
	UE_LOG(LogArcPlayerState
		, Log
		, TEXT("[Client] ArcPS %s assigned team %d")
		, *GetUniqueId().ToDebugString()
		, MyTeamId);
	
	OnTeamChangedDelegate.Broadcast(OldTeamId
		, MyTeamId);
}

// void AArcCorePlayerState::ClientBroadcastMessage_Implementation(const FArcVerbMessage
// Message)
//{
//	// This check is needed to prevent running the action when in standalone mode
//	if (GetNetMode() == NM_Client)
//	{
//		UGameplayMessageSubsystem::Get(this).BroadcastMessage(Message.Verb, Message);
//	}
// }
