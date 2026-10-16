// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Logging/LogVerbosity.h"
#include "Mass/EntityHandle.h"
#include "UObject/UObjectGlobals.h"

class UArcEffectDefinition;
struct FLogCategoryBase;
struct FMassEntityManager;

namespace ArcMechTest
{
	ARCMASSMECHANICSTEST_API FString MakeTestAssetPath(const TCHAR* MountRoot, const TCHAR* Path);

	// Fails the running automation test (or logs an error outside one) instead of crashing the process.
	ARCMASSMECHANICSTEST_API void ReportMissingTestAsset(const FString& ObjectPath, const UClass* ExpectedClass);

	// Returns null and fails the current test when the asset is missing; callers must assert the result before using it.
	template <typename T>
	T* LoadTestAssetFrom(const TCHAR* MountRoot, const TCHAR* Path)
	{
		const FString ObjectPath = MakeTestAssetPath(MountRoot, Path);
		T* Asset = LoadObject<T>(nullptr, *ObjectPath);
		if (!Asset)
		{
			ReportMissingTestAsset(ObjectPath, T::StaticClass());
		}
		return Asset;
	}

	ARCMASSMECHANICSTEST_API int32 GetOwnedTagCount(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FGameplayTag& Tag);

	ARCMASSMECHANICSTEST_API int32 CountActiveEffects(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UArcEffectDefinition* Definition);

	template <typename T>
	T* LoadTestAsset(const TCHAR* Path)
	{
		return LoadTestAssetFrom<T>(TEXT("ArcMassMechanicsTest"), Path);
	}

	// Hold in a TOptional and Emplace it per test (harness Initialize or BEFORE_EACH): CQTest also constructs each test class at module load.
	class ARCMASSMECHANICSTEST_API FVerboseLogScope
	{
	public:
		explicit FVerboseLogScope(const TCHAR* CategoryName = TEXT("LogArcMassMechanics"));
		~FVerboseLogScope();
		FVerboseLogScope(const FVerboseLogScope&) = delete;
		FVerboseLogScope& operator=(const FVerboseLogScope&) = delete;

	private:
		FLogCategoryBase* Category = nullptr;
		ELogVerbosity::Type PreviousVerbosity = ELogVerbosity::Log;
	};
}
