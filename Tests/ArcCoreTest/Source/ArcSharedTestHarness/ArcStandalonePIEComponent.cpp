// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcStandalonePIEComponent.h"

#if ENABLE_ARC_STANDALONE_PIE_TEST

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcStandalonePIE, Log, All);

FArcStandalonePIEComponent::FArcStandalonePIEComponent(
	FAutomationTestBase* InTestRunner,
	FTestCommandBuilder& InCommandBuilder,
	bool bIsInitializing,
	TSubclassOf<AGameModeBase> InGameMode,
	FSoftClassPath InGameInstanceClass,
	TOptional<FTimespan> Timeout)
	: TestRunner(InTestRunner)
	, CommandBuilder(&InCommandBuilder)
	, GameMode(InGameMode)
	, GameInstanceClass(InGameInstanceClass)
{
	if (bIsInitializing)
	{
		return;
	}

	StateRestorer = FPIENetworkTestStateRestorer{GameInstanceClass, GameMode};

	const FTimespan TimeoutValue = MakeTimeout(Timeout);
	CommandBuilder
		->Do(TEXT("Stop PIE"), [this]() { StopPie(); })
		.Then(TEXT("Load Or Create Map"), [this]()
			{
				if (MapOverride.IsValid())
				{
					FAutomationEditorCommonUtils::LoadMap(MapOverride.ToString());
				}
				else
				{
					FAutomationEditorCommonUtils::CreateNewMap();
				}
			})
		.Then(TEXT("Start PIE"), [this]() { StartPie(); })
		.Until(TEXT("Set World"), [this]() { return SetWorld(); }, TimeoutValue)
		.OnTearDown(TEXT("Restore Editor State"), [this]() { RestoreState(); });
}

FArcStandalonePIEComponent& FArcStandalonePIEComponent::WithURLOptions(const FString& InURLOptions)
{
	URLOptions = InURLOptions;
	return *this;
}

FArcStandalonePIEComponent& FArcStandalonePIEComponent::WithMap(FSoftObjectPath InMapPath)
{
	MapOverride = MoveTemp(InMapPath);
	return *this;
}

FArcStandalonePIEComponent& FArcStandalonePIEComponent::Then(TFunction<void()> Action)
{
	CommandBuilder->Then(Action);
	return *this;
}

FArcStandalonePIEComponent& FArcStandalonePIEComponent::Then(const TCHAR* Description, TFunction<void()> Action)
{
	CommandBuilder->Then(Description, Action);
	return *this;
}

FArcStandalonePIEComponent& FArcStandalonePIEComponent::Do(TFunction<void()> Action)
{
	CommandBuilder->Do(Action);
	return *this;
}

FArcStandalonePIEComponent& FArcStandalonePIEComponent::Do(const TCHAR* Description, TFunction<void()> Action)
{
	CommandBuilder->Do(Description, Action);
	return *this;
}

FArcStandalonePIEComponent& FArcStandalonePIEComponent::Until(TFunction<bool()> Query, TOptional<FTimespan> Timeout)
{
	const FTimespan TimeoutValue = MakeTimeout(Timeout);
	CommandBuilder->Until(Query, TimeoutValue);
	return *this;
}

FArcStandalonePIEComponent& FArcStandalonePIEComponent::Until(const TCHAR* Description, TFunction<bool()> Query, TOptional<FTimespan> Timeout)
{
	const FTimespan TimeoutValue = MakeTimeout(Timeout);
	CommandBuilder->Until(Description, Query, TimeoutValue);
	return *this;
}

void FArcStandalonePIEComponent::StopPie()
{
	if (GUnrealEd != nullptr && GUnrealEd->PlayWorld != nullptr)
	{
		GUnrealEd->RequestEndPlayMap();
	}
}

void FArcStandalonePIEComponent::StartPie()
{
	ULevelEditorPlaySettings* PlaySettings = NewObject<ULevelEditorPlaySettings>();
	PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	PlaySettings->SetPlayNumberOfClients(1);
	PlaySettings->bLaunchSeparateServer = false;
	PlaySettings->GameGetsMouseControl = false;
	PlaySettings->SetRunUnderOneProcess(true);

	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	FRequestPlaySessionParams SessionParams;
	SessionParams.WorldType = EPlaySessionWorldType::PlayInEditor;
	SessionParams.DestinationSlateViewport = LevelEditorModule.GetFirstActiveViewport();
	SessionParams.EditorPlaySettings = PlaySettings;
	SessionParams.GameModeOverride = (GameMode != nullptr) ? GameMode : TSubclassOf<AGameModeBase>(AGameModeBase::StaticClass());

	if (!URLOptions.IsEmpty())
	{
		// PIE_Standalone single-process has no first-class URL-options field on the params.
		// GlobalMapOverride is parsed as an FURL by GameInstance::InitializeForPlayInEditor;
		// the map portion is overwritten with the PIE map but query options after `?` are
		// preserved into the FURL passed to LoadMap. Prefix any leading `?` so a bare
		// "Experience=Foo" caller works the same as "?Experience=Foo".
		// TODO: revisit once UE exposes a direct URL-options pass-through for in-process PIE.
		FString Options = URLOptions;
		if (!Options.StartsWith(TEXT("?")))
		{
			Options = FString::Printf(TEXT("?%s"), *Options);
		}
		SessionParams.GlobalMapOverride = FString::Printf(TEXT("/Temp/Untitled%s"), *Options);
	}

	GUnrealEd->RequestPlaySession(SessionParams);
	GUnrealEd->StartQueuedPlaySessionRequest();
}

bool FArcStandalonePIEComponent::SetWorld()
{
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType != EWorldType::PIE)
		{
			continue;
		}

		UWorld* World = WorldContext.World();
		if (!IsValid(World))
		{
			continue;
		}

		State.World = World;
		break;
	}

	if (State.World == nullptr)
	{
		return false;
	}

	State.PC = GEngine->GetFirstLocalPlayerController(State.World);
	return State.PC != nullptr;
}

void FArcStandalonePIEComponent::RestoreState()
{
	if (GUnrealEd != nullptr)
	{
		GUnrealEd->RequestEndPlayMap();
	}
	StateRestorer.Restore();
	State.World = nullptr;
	State.PC = nullptr;
}

FTimespan FArcStandalonePIEComponent::MakeTimeout(TOptional<FTimespan> Timeout) const
{
	if (Timeout.IsSet())
	{
		return Timeout.GetValue();
	}
	else if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
	{
		return FTimespan::FromSeconds(ConsoleVariable->GetFloat());
	}
	else
	{
		UE_LOGF(LogArcStandalonePIE, Warning, "CVar '%ls' was not found. Defaulting to %f seconds.", CQTestConsoleVariables::NetworkTimeoutName, CQTestConsoleVariables::NetworkTimeout);
		return FTimespan::FromSeconds(CQTestConsoleVariables::NetworkTimeout);
	}
}

#endif // ENABLE_ARC_STANDALONE_PIE_TEST
