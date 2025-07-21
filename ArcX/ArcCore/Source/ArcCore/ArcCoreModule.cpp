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

#include "ArcCoreModule.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "ArcCore/AbilitySystem/GameplayDebuggerCategory_Attributes.h"
#include "GameplayDebugger.h"
#endif

#include "ArcCoreGameInstance.h"
#include "GameFeaturesSubsystemSettings.h"
#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "Core/ArcCoreAssetManager.h"
#include "Development/ArcDeveloperSettings.h"
#include "GameMode/ArcExperienceData.h"
#include "GameMode/ArcUserFacingExperienceDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FArcGameCoreModule"

namespace Arcx
{
	FPrimaryAssetId GetExperiencePrimaryId(UWorld* InWorld, FString& OutExperienceIdSource)
	{
		FPrimaryAssetId ExperienceId;
		FString ExperienceIdSource;

		// Precedence order (highest wins)
		//  - Currently selected game instance option. 
		//  - URL Options override
		//  - Matchmaking assignment (if present) (UserExperience)
		//  - Command Line override
		//  - World Settings
		//  - Developer Settings (PIE only)

		UArcCoreGameInstance* ArcCoreGameInstance = Cast<UArcCoreGameInstance>(InWorld->GetGameInstance());
		if (ArcCoreGameInstance && ArcCoreGameInstance->GetSelectedGameExperienceId().IsValid())
		{
			ExperienceId = ArcCoreGameInstance->GetSelectedGameExperienceId();
			ExperienceIdSource = TEXT("Game Instance");
		}
		
		FString OptionsString(TEXT(""));
		FURL InURL = InWorld->URL;
		for( int32 i=0; i<InURL.Op.Num(); i++ )
		{
			OptionsString += TEXT("?");
			OptionsString += InURL.Op[i];
		}
		
		UWorld* World = InWorld;
		
		if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
		{
			const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));
			ExperienceId = FPrimaryAssetId(FPrimaryAssetType(UArcExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromOptions));
			ExperienceIdSource = TEXT("OptionsString");
		}
	
		// "UserExperience="
		if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("UserExperience")))
		{
			const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("UserExperience"));
			UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();
			FPrimaryAssetId UserExperienceId = FPrimaryAssetId::FromString(ExperienceFromOptions);
			
			TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAsset(UserExperienceId);
			if (ensure(Handle.IsValid()))
			{
				Handle->WaitUntilComplete();
			}
			
			UArcUserFacingExperienceDefinition* UserFacing = Cast<UArcUserFacingExperienceDefinition>(Handle->GetLoadedAsset());
			ExperienceIdSource = "User Facing Experience";
			ExperienceId = UserFacing->ExperienceID;
			
		}
		
		// see if the command line wants to set the experience
		if (!ExperienceId.IsValid())
		{
			FString ExperienceFromCommandLine;
			if (FParse::Value(FCommandLine::Get(), TEXT("Experience="), ExperienceFromCommandLine))
			{
				ExperienceId = FPrimaryAssetId::ParseTypeAndName(ExperienceFromCommandLine);
				if (!ExperienceId.PrimaryAssetType.IsValid())
				{
					ExperienceId = FPrimaryAssetId(FPrimaryAssetType(UArcExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromCommandLine));
				}
				ExperienceIdSource = TEXT("CommandLine");
			}
		}
	
		// see if the world settings has a default experience
		if (!ExperienceId.IsValid())
		{
			if (AArcCoreWorldSettings* TypedWorldSettings = Cast<AArcCoreWorldSettings>(World->GetWorldSettings()))
			{
				ExperienceId = TypedWorldSettings->GetDefaultExperience();
				ExperienceIdSource = TEXT("WorldSettings");
			}
		}
		
	#if WITH_EDITOR
		if (!ExperienceId.IsValid() && World->IsPlayInEditor())
		{
			ExperienceId = GetDefault<UArcDeveloperSettings>()->ExperienceOverride;
			ExperienceIdSource = TEXT("DeveloperSettings");
		}
	#endif
		
		OutExperienceIdSource = ExperienceIdSource; 
		return ExperienceId;
	}

}
void FArcCoreModule::OnAllModuleLoadingPhasesComplete()
{
}

void FArcCoreModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("Attributes"
		, IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_Attributes::MakeInstance));
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif // WITH_GAMEPLAY_DEBUGGER

	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FArcCoreModule::OnAllModuleLoadingPhasesComplete);
	
	// This code will execute after your module is loaded into memory; the exact timing
	// is specified in the .uplugin file per-module

	FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* World, const UWorld::InitializationValues IVS)
	{
		if (World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game))
		{
			UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();
			FString Source;
			FPrimaryAssetId ExperienceId = Arcx::GetExperiencePrimaryId(World, Source);
			UArcExperienceDefinition* CurrentExperience = UArcCoreAssetManager::GetAsset<UArcExperienceDefinition>(ExperienceId);
			if (CurrentExperience == nullptr)
			{
				return;
			}
			
			TSet<FPrimaryAssetId> BundleAssetList;
			TSet<FSoftObjectPath> RawAssetList;
		
			for (const TObjectPtr<UArcExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
			{
				if (ActionSet != nullptr)
				{
					BundleAssetList.Add(ActionSet->GetPrimaryAssetId());
				}
			}
		
			BundleAssetList.Add(CurrentExperience->GetPrimaryAssetId());
		
			// Load and activate game features associated with the experience
		
			TArray<FName> BundlesToLoad;
			BundlesToLoad.Add(FArcBundles::Equipped);
		
			//@TODO: Centralize this client/server stuff into the ArcAssetManager
			const ENetMode OwnerNetMode = World->GetNetMode();
			const bool bLoadClient = GIsEditor || (OwnerNetMode != NM_DedicatedServer);
			const bool bLoadServer = GIsEditor || (OwnerNetMode != NM_Client);

			BundlesToLoad.Append(CurrentExperience->BundlesState.GetBundles(OwnerNetMode));
			if (bLoadClient)
			{
				BundlesToLoad.AddUnique(UGameFeaturesSubsystemSettings::LoadStateClient);
			}
			if (bLoadServer)
			{
				BundlesToLoad.AddUnique(UGameFeaturesSubsystemSettings::LoadStateServer);
			}

			TSharedPtr<FStreamableHandle> BundleLoadHandle = nullptr;
			if (BundleAssetList.Num() > 0)
			{
				BundleLoadHandle = AssetManager.ChangeBundleStateForPrimaryAssets(
					BundleAssetList.Array()
					, {}
					, {}
					, false
					, FStreamableDelegate()
					, FStreamableManager::AsyncLoadHighPriority);
			}
		
			TSharedPtr<FStreamableHandle> RawLoadHandle = nullptr;
			if (RawAssetList.Num() > 0)
			{
				RawLoadHandle = AssetManager.LoadAssetList(RawAssetList.Array()
					, FStreamableDelegate()
					, FStreamableManager::AsyncLoadHighPriority
					, TEXT("StartExperienceLoad()"));
			}
			
			// If both async loads are running, combine them
			TSharedPtr<FStreamableHandle> Handle = nullptr;
			if (BundleLoadHandle.IsValid() && RawLoadHandle.IsValid())
			{
				Handle = AssetManager.GetStreamableManager().CreateCombinedHandle({BundleLoadHandle, RawLoadHandle});
			}
			else
			{
				Handle = BundleLoadHandle.IsValid() ? BundleLoadHandle : RawLoadHandle;
			}
		
			//FStreamableDelegate OnAssetsLoadedDelegate = FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceLoadComplete);
			FStreamableDelegate OnAssetsLoadedDelegate = FStreamableDelegate::CreateLambda([](){});
			if (!Handle.IsValid() || Handle->HasLoadCompleted())
			{
				// Assets were already loaded, call the delegate now
				FStreamableHandle::ExecuteDelegate(OnAssetsLoadedDelegate);
			}
			else
			{
				Handle->BindCompleteDelegate(OnAssetsLoadedDelegate);
		
				Handle->BindCancelDelegate(FStreamableDelegate::CreateLambda([OnAssetsLoadedDelegate]()
				{
					OnAssetsLoadedDelegate.ExecuteIfBound();
				}));
			}
		
			// This set of assets gets preloaded, but we don't block the start of the experience
			// based on it
			TSet<FPrimaryAssetId> PreloadAssetList;
			//@TODO: Determine assets to preload (but not blocking-ly)
			if (PreloadAssetList.Num() > 0)
			{
				AssetManager.ChangeBundleStateForPrimaryAssets(PreloadAssetList.Array()
					, BundlesToLoad
					, {});
			}
		
			// Block loading. there is actually not reason to do anything before experience fully loads.
			if (Handle.IsValid())
			{
				Handle->WaitUntilComplete();	
			}
			
			UArcCoreAssetManager::Get().SetExperienceBundlesSync(World, BundlesToLoad);
		}
	});
}

void FArcCoreModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules
	// that support dynamic reloading, we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcCoreModule, ArcCore)


