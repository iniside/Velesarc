// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class IAssetTypeActions;

class FArcTacticsEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<IAssetTypeActions> AssetTypeActions;
	FDelegateHandle ToolsetRegistrationHandle;
};
