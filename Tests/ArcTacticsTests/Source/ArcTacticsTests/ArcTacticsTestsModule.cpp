#include "Modules/ModuleManager.h"

class FArcTacticsTestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FArcTacticsTestsModule, ArcTacticsTests)
