// Copyright Lukasz Baran. All Rights Reserved.

#include "Thread/ArcRmlAsyncJobs.h"

FArcRmlAsyncJobs::FArcRmlAsyncJobs(bool bInInline, TFunction<void()>&& InOnTaskFinished)
	: OnTaskFinished(MoveTemp(InOnTaskFinished))
	, bInline(bInInline)
{
}

void FArcRmlAsyncJobs::Launch(Rml::Function<void()> Task)
{
	if (bInline)
	{
		Task();
		return;
	}

	Tasks.RemoveAll([](const UE::Tasks::FTask& Launched) { return Launched.IsCompleted(); });
	Tasks.Add(UE::Tasks::Launch(TEXT("ArcRmlAsyncJob"),
		[this, Job = MoveTemp(Task)]() mutable
		{
			Job();
			OnTaskFinished();
		}));
}

void FArcRmlAsyncJobs::WaitForAll()
{
	for (UE::Tasks::FTask& Launched : Tasks)
	{
		Launched.Wait();
	}
	Tasks.Reset();
}
