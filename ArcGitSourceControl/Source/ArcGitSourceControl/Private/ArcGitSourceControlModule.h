// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "ArcGitSourceControlSettings.h"
#include "ArcGitSourceControlProvider.h"

class FArcGitSourceControlModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Access the Git source control settings */
	FArcGitSourceControlSettings& AccessSettings()
	{
		return ArcGitSourceControlSettings;
	}

	const FArcGitSourceControlSettings& AccessSettings() const
	{
		return ArcGitSourceControlSettings;
	}

	/** Save the Git source control settings */
	void SaveSettings();

	/** Access the Git source control provider */
	FArcGitSourceControlProvider& GetProvider()
	{
		return ArcGitSourceControlProvider;
	}

	const FArcGitSourceControlProvider& GetProvider() const
	{
		return ArcGitSourceControlProvider;
	}

private:
	/** The Git source control provider */
	FArcGitSourceControlProvider ArcGitSourceControlProvider;

	/** The settings for Git source control */
	FArcGitSourceControlSettings ArcGitSourceControlSettings;
};
