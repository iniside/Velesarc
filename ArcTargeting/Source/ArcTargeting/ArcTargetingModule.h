#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "ArcTargetingPipelineTypes.h"
#include "VisualLogger/VisualLoggerDefines.h"

class IConsoleObject;

ARCTARGETING_API DECLARE_LOG_CATEGORY_EXTERN(LogArcTargeting, Log, All);

class FArcTargetingModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#if UE_DEBUG_RENDERING_ENABLED
	/** Most recent preview request handle, owned by the module so the next preview can release it. */
	FArcTargetingHandle PreviewHandle;
	TWeakObjectPtr<class UWorld> PreviewWorld;

	IConsoleObject*PreviewCommand = nullptr;
	IConsoleObject*ClearCommand = nullptr;
	IConsoleObject*ListCommand = nullptr;
#endif
};
