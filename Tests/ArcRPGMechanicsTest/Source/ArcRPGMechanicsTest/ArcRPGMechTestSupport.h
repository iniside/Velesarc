// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMechTestSupport.h"

namespace ArcRPGMechTest
{
	template <typename T>
	T* LoadTestAsset(const TCHAR* Path)
	{
		return ArcMechTest::LoadTestAssetFrom<T>(TEXT("ArcRPGMechanicsTest"), Path);
	}

	// Hold in a TOptional and Emplace it per test (harness Initialize or BEFORE_EACH): CQTest also constructs each test class at module load.
	class FVerboseLogScope
	{
	public:
		FVerboseLogScope();

	private:
		ArcMechTest::FVerboseLogScope MechanicsScope;
		ArcMechTest::FVerboseLogScope RPGScope;
	};
}
