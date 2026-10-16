// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "UObject/ObjectKey.h"

class UArcTQSQueryDefinition;

class FArcTargetQueryEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void HandleDefinitionCompiled(const UArcTQSQueryDefinition& Definition);

	FDelegateHandle PostCompileHandle;
	FDelegateHandle ToolsetRegistrationHandle;
	TMap<TObjectKey<UArcTQSQueryDefinition>, uint32> ReportedDiagnostics;
};
