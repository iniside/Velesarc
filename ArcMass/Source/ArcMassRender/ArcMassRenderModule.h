// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FArcMassRenderModule : public IModuleInterface
{
#if WITH_EDITOR
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	/** Handle for the FWorldDelegates::OnWorldCleanup binding that strips componentless render proxies
	 *  from a dying editor world's FScene before it is released. */
	FDelegateHandle WorldCleanupHandle;
#endif // WITH_EDITOR
};
