// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/Task.h"

#include "RmlUi/Core/AsyncJobInterface.h"

class FArcRmlAsyncJobs final : public Rml::AsyncJobInterface
{
public:
	FArcRmlAsyncJobs(bool bInInline, TFunction<void()>&& InOnTaskFinished);

	virtual void Launch(Rml::Function<void()> Task) override;
	void WaitForAll();

private:
	TFunction<void()> OnTaskFinished;
	TArray<UE::Tasks::FTask> Tasks;
	bool bInline = false;
};
