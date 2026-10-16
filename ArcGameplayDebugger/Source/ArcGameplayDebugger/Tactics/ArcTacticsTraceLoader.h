// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

struct FArcTacticsRecording;

class FArcTacticsTraceLoader
{
public:
	FArcTacticsTraceLoader();
	~FArcTacticsTraceLoader();

	FArcTacticsTraceLoader(const FArcTacticsTraceLoader&) = delete;
	FArcTacticsTraceLoader& operator=(const FArcTacticsTraceLoader&) = delete;

	bool Start(const FString& FilePath, FString& OutError);
	bool IsLoading() const;
	bool IsFinished() const;
	TUniquePtr<FArcTacticsRecording> TakeResult();
	void Cancel();

	static ARCGAMEPLAYDEBUGGER_API TUniquePtr<FArcTacticsRecording> LoadBlocking(const FString& FilePath, FString& OutError);

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};
