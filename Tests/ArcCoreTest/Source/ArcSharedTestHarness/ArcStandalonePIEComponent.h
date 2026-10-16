// Copyright Lukasz Baran. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"

#define ENABLE_ARC_STANDALONE_PIE_TEST WITH_EDITOR && (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

#if ENABLE_ARC_STANDALONE_PIE_TEST

#include "CQTest.h"
#include "CQTestSettings.h"
#include "Components/PIENetworkTestStateRestorer.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPath.h"

class AGameModeBase;
class APlayerController;
class UWorld;
class FAutomationTestBase;

/**
 * State holder for the standalone PIE harness. Mirrors FBasePIENetworkComponentState
 * but trims everything network-related — single PIE world, single local PC.
 *
 * `PC` is the generic engine type. Tests that need a project-specific subclass
 * (e.g. AArcCorePlayerController) cast at the use site.
 */
struct ARCSHAREDTESTHARNESS_API FArcStandalonePIEState
{
	UWorld* World = nullptr;
	APlayerController* PC = nullptr;
};

/**
 * Single-PIE test harness modeled on FBasePIENetworkComponent without networking.
 * Spawns a PIE_Standalone session with a GameModeOverride so
 * GEngine->GetFirstLocalPlayerController returns the supplied PC class.
 */
class ARCSHAREDTESTHARNESS_API FArcStandalonePIEComponent
{
public:
	FArcStandalonePIEComponent(
		FAutomationTestBase* InTestRunner,
		FTestCommandBuilder& InCommandBuilder,
		bool bIsInitializing,
		TSubclassOf<AGameModeBase> InGameMode,
		FSoftClassPath InGameInstanceClass = FSoftClassPath{},
		TOptional<FTimespan> Timeout = CQTest::DefaultTimeout);

	/**
	 * Builder: append URL options (e.g. "?Experience=Foo") to the play session URL.
	 * Must be called before the harness's queued StartPie step runs — i.e. immediately
	 * after construction, while the test is still in BEFORE_EACH/Setup.
	 */
	FArcStandalonePIEComponent& WithURLOptions(const FString& InURLOptions);

	/**
	 * Builder: load a specific map before starting PIE instead of the default
	 * blank `/Temp/Untitled` map produced by `FAutomationEditorCommonUtils::CreateNewMap()`.
	 * Must be called immediately after construction, before the harness's queued
	 * steps execute (i.e. while still in BEFORE_EACH/Setup).
	 */
	FArcStandalonePIEComponent& WithMap(FSoftObjectPath InMapPath);

	FArcStandalonePIEComponent& Then(TFunction<void()> Action);
	FArcStandalonePIEComponent& Then(const TCHAR* Description, TFunction<void()> Action);
	FArcStandalonePIEComponent& Do(TFunction<void()> Action);
	FArcStandalonePIEComponent& Do(const TCHAR* Description, TFunction<void()> Action);
	FArcStandalonePIEComponent& Until(TFunction<bool()> Query, TOptional<FTimespan> Timeout = CQTest::DefaultTimeout);
	FArcStandalonePIEComponent& Until(const TCHAR* Description, TFunction<bool()> Query, TOptional<FTimespan> Timeout = CQTest::DefaultTimeout);

	FArcStandalonePIEState& GetState() { return State; }

private:
	void StopPie();
	void StartPie();
	bool SetWorld();
	void RestoreState();
	FTimespan MakeTimeout(TOptional<FTimespan> Timeout) const;

	FArcStandalonePIEState State;
	FAutomationTestBase* TestRunner = nullptr;
	FTestCommandBuilder* CommandBuilder = nullptr;
	TSubclassOf<AGameModeBase> GameMode;
	FSoftClassPath GameInstanceClass;
	FString URLOptions;
	FSoftObjectPath MapOverride;
	FPIENetworkTestStateRestorer StateRestorer;
};

#endif // ENABLE_ARC_STANDALONE_PIE_TEST
