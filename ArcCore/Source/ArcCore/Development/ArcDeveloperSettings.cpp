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

#include "ArcDeveloperSettings.h"
#include "CommonUIVisibilitySubsystem.h"
#include "Engine/PlatformSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/App.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ArcDeveloperSettings"

UArcDeveloperSettings::UArcDeveloperSettings()
{
}

FName UArcDeveloperSettings::GetCategoryName() const
{
	return FApp::GetProjectName();
}

#if WITH_EDITOR
void UArcDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplySettings();
}

void UArcDeveloperSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);

	ApplySettings();
}

void UArcDeveloperSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ApplySettings();
}

void UArcDeveloperSettings::ApplySettings()
{
	UCommonUIVisibilitySubsystem::SetDebugVisibilityConditions(AdditionalPlatformTraitsToEnable
		, AdditionalPlatformTraitsToSuppress);

	if (PretendPlatform != LastAppliedPretendPlatform)
	{
		ChangeActivePretendPlatform(PretendPlatform);
	}
}

void UArcDeveloperSettings::ChangeActivePretendPlatform(FName NewPlatformName)
{
	LastAppliedPretendPlatform = NewPlatformName;
	PretendPlatform = NewPlatformName;

	// ChangeScalabilityPreviewPlatform(FName NewPlatformScalabilityName)
	// UPlatformSettings::SetEditorSimulatedPlatform(PretendPlatform);
}

#endif

TArray<FName> UArcDeveloperSettings::GetKnownPlatformIds() const
{
	TArray<FName> Results;

#if WITH_EDITOR
	Results.Add(NAME_None);
	// Results.Append(UPlatformSettings::GetKnownAndEnablePlatformIniNames());
#endif

	return Results;
}

#if WITH_EDITOR
void UArcDeveloperSettings::OnPlayInEditorStarted() const
{
	// Show a notification toast to remind the user that there's an experience override
	// set
	if (ExperienceOverride.IsValid())
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("ExperienceOverrideActive"
				, "Developer Settings Override\nExperience {0}")
			, FText::FromName(ExperienceOverride.PrimaryAssetName)));
		Info.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}
#endif

#undef LOCTEXT_NAMESPACE
