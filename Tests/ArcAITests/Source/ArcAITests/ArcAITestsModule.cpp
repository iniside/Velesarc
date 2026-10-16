// Copyright Lukasz Baran. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FArcAITestsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override {};
	virtual void ShutdownModule() override {};
};


IMPLEMENT_MODULE(FArcAITestsModule, ArcAITests);
