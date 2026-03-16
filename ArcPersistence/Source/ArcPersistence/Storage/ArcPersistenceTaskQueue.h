/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#pragma once
#include "CoreMinimal.h"
#include "Tasks/Task.h"

class ARCPERSISTENCE_API FArcPersistenceTaskQueue
{
public:
	template<typename ResultType>
	TFuture<ResultType> Enqueue(TUniqueFunction<ResultType()> Work);

	void Flush();
	bool HasPendingWork() const;

private:
	UE::Tasks::FTask LastTask;
	FCriticalSection QueueLock;
};

// Template implementation
template<typename ResultType>
TFuture<ResultType> FArcPersistenceTaskQueue::Enqueue(TUniqueFunction<ResultType()> Work)
{
	auto Promise = MakeShared<TPromise<ResultType>>();
	TFuture<ResultType> Future = Promise->GetFuture();

	FScopeLock Lock(&QueueLock);

	TArray<UE::Tasks::FTask> Prerequisites;
	if (LastTask.IsValid())
	{
		Prerequisites.Add(LastTask);
	}

	LastTask = UE::Tasks::Launch(
		UE_SOURCE_LOCATION,
		[Promise, Work = MoveTemp(Work)]() mutable
		{
			ResultType Result = Work();
			Promise->SetValue(MoveTemp(Result));
		},
		Prerequisites,
		UE::Tasks::ETaskPriority::BackgroundNormal
	);

	return Future;
}
