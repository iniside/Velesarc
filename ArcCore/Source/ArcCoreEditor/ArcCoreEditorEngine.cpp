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

#include "ArcCoreEditorEngine.h"

#include "Development/ArcDeveloperSettings.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameMode/ArcCoreWorldSettings.h"
#include "Settings/ContentBrowserSettings.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "LyraEditor"

UArcCoreEditorEngine::UArcCoreEditorEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcCoreEditorEngine::Init(IEngineLoop* InEngineLoop)
{
	Super::Init(InEngineLoop);
}

void UArcCoreEditorEngine::Start()
{
	Super::Start();
}

void UArcCoreEditorEngine::Tick(float DeltaSeconds
								, bool bIdleMode)
{
	Super::Tick(DeltaSeconds
		, bIdleMode);

	FirstTickSetup();
}

void UArcCoreEditorEngine::FirstTickSetup()
{
	if (bFirstTickSetup)
	{
		return;
	}

	bFirstTickSetup = true;

	// Force show plugin content on load.
	GetMutableDefault<UContentBrowserSettings>()->SetDisplayPluginFolders(true);

	// Set PIE to default to Shift+Escape if no user overrides have been set.
	{
		FInputChord OutPrimaryChord, OutSecondaryChord;
		FInputBindingManager::Get().GetUserDefinedChord("PlayWorld"
			, "StopPlaySession"
			, EMultipleKeyBindingIndex::Primary
			, OutPrimaryChord);
		FInputBindingManager::Get().GetUserDefinedChord("PlayWorld"
			, "StopPlaySession"
			, EMultipleKeyBindingIndex::Secondary
			, OutSecondaryChord);

		// Is there already a user setting for stopping PIE?  Then don't do this.
		if (!(OutPrimaryChord.IsValidChord() || OutSecondaryChord.IsValidChord()))
		{
			TSharedPtr<FUICommandInfo> StopCommand = FInputBindingManager::Get().FindCommandInContext("PlayWorld"
				, "StopPlaySession");
			if (ensure(StopCommand))
			{
				// Shift+Escape to exit PIE.  Some folks like Ctrl+Q, if that's the case,
				// change it here for your team.
				StopCommand->SetActiveChord(FInputChord(EKeys::Escape
						, true
						, false
						, false
						, false)
					, EMultipleKeyBindingIndex::Primary);
				FInputBindingManager::Get().NotifyActiveChordChanged(*StopCommand
					, EMultipleKeyBindingIndex::Primary);
			}
		}
	}
}

FGameInstancePIEResult UArcCoreEditorEngine::PreCreatePIEInstances(const bool bAnyBlueprintErrors
																   , const bool bStartInSpectatorMode
																   , const float PIEStartTime
																   , const bool bSupportsOnlinePIE
																   , int32& InNumOnlinePIEInstances)
{
	if (const AArcCoreWorldSettings* LyraWorldSettings = Cast<AArcCoreWorldSettings>(EditorWorld->GetWorldSettings()))
	{
		if (LyraWorldSettings->ForceStandaloneNetMode)
		{
			EPlayNetMode OutPlayNetMode;
			PlaySessionRequest->EditorPlaySettings->GetPlayNetMode(OutPlayNetMode);
			if (OutPlayNetMode != PIE_Standalone)
			{
				PlaySessionRequest->EditorPlaySettings->SetPlayNetMode(PIE_Standalone);

				FNotificationInfo Info(LOCTEXT("ForcingStandaloneForFrontend"
					, "Forcing NetMode: Standalone for the Frontend"));
				Info.ExpireDuration = 2.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
		}
	}

	//@TODO: Should add delegates that a *non-editor* module could bind to for PIE
	//start/stop instead of poking directly
	GetDefault<UArcDeveloperSettings>()->OnPlayInEditorStarted();
	// GetDefault<ULyraPlatformEmulationSettings>()->OnPlayInEditorStarted();

	//
	FGameInstancePIEResult Result = Super::PreCreatePIEServerInstance(bAnyBlueprintErrors
		, bStartInSpectatorMode
		, PIEStartTime
		, bSupportsOnlinePIE
		, InNumOnlinePIEInstances);

	return Result;
}

#undef LOCTEXT_NAMESPACE
