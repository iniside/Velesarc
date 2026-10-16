// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcEnviroEffectsTestHarness.h"
#include "ArcEnviroEffects/ArcEnviroEffectsSubsystem.h"
#include "ArcEnviroEffects/ArcEnviroVolume.h"
#include "Components/ActorTestSpawner.h"
#include "UObject/UnrealType.h"

namespace ArcEnviroVolumeTestsPrivate
{
	const FVector Inside = FVector::ZeroVector;
	const FVector Outside = FVector(5000.f, 0.f, 0.f);

	struct FVolumeSetup
	{
		TOptional<int32> Rain;
		TOptional<int32> Cold;
		TOptional<int32> Heat;
		int32 Priority = 0;
	};

	bool SetBool(AArcEnviroVolume& Volume, const TCHAR* Name, bool bValue)
	{
		FBoolProperty* Property = FindFProperty<FBoolProperty>(AArcEnviroVolume::StaticClass(), FName(Name));
		if (!Property)
		{
			return false;
		}
		Property->SetPropertyValue_InContainer(&Volume, bValue);
		return true;
	}

	bool SetInt(AArcEnviroVolume& Volume, const TCHAR* Name, int32 Value)
	{
		FIntProperty* Property = FindFProperty<FIntProperty>(AArcEnviroVolume::StaticClass(), FName(Name));
		if (!Property)
		{
			return false;
		}
		Property->SetPropertyValue_InContainer(&Volume, Value);
		return true;
	}

	bool ConfigureAxis(AArcEnviroVolume& Volume, const TCHAR* ToggleName, const TCHAR* LevelName, const TOptional<int32>& Level)
	{
		if (!Level.IsSet())
		{
			return true;
		}
		return SetBool(Volume, ToggleName, true) && SetInt(Volume, LevelName, Level.GetValue());
	}

	AArcEnviroVolume* SpawnVolume(FActorTestSpawner& Spawner, const FVector& Location, const FVolumeSetup& Setup)
	{
		AArcEnviroVolume& Volume = Spawner.SpawnActorAt<AArcEnviroVolume>(Location, FRotator::ZeroRotator);
		const bool bConfigured = ConfigureAxis(Volume, TEXT("bOverrideRain"), TEXT("RainLevel"), Setup.Rain)
			&& ConfigureAxis(Volume, TEXT("bOverrideCold"), TEXT("ColdLevel"), Setup.Cold)
			&& ConfigureAxis(Volume, TEXT("bOverrideHeat"), TEXT("HeatLevel"), Setup.Heat)
			&& SetInt(Volume, TEXT("Priority"), Setup.Priority);
		if (!bConfigured)
		{
			return nullptr;
		}
		if (!Volume.HasActorBegunPlay())
		{
			Volume.DispatchBeginPlay();
		}
		return &Volume;
	}

	constexpr int32 Unset = -1;

	TOptional<int32> ToOptional(int32 Level)
	{
		return Level == Unset ? TOptional<int32>() : TOptional<int32>(Level);
	}

	FVolumeSetup MakeSetup(int32 Rain, int32 Cold, int32 Heat, int32 Priority)
	{
		FVolumeSetup Setup;
		Setup.Rain = ToOptional(Rain);
		Setup.Cold = ToOptional(Cold);
		Setup.Heat = ToOptional(Heat);
		Setup.Priority = Priority;
		return Setup;
	}
}

TEST_CLASS(ArcEnviroVolumes, "ArcEnviroEffects.Volumes")
{
	FActorTestSpawner Spawner;
	ArcEnviroTest::FHarness Harness;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		const bool bInitialized = Harness.Initialize(Spawner);
		check(bInitialized);
	}

	TEST_METHOD(Volume_RegistersOnBeginPlay_AndUnregistersOnDestroy)
	{
		using namespace ArcEnviroVolumeTestsPrivate;

		AArcEnviroVolume* Volume = SpawnVolume(Spawner, FVector::ZeroVector, MakeSetup(3, Unset, Unset, 0));
		ASSERT_THAT(IsNotNull(Volume));

		const FArcEnviroLevelOverrides Before = Harness.Enviro->GetOverridesAtLocation(Inside);
		ASSERT_THAT(IsTrue(Before.Rain.IsSet() && Before.Rain.GetValue() == 3));

		const FArcEnviroLevelOverrides Away = Harness.Enviro->GetOverridesAtLocation(Outside);
		ASSERT_THAT(IsFalse(Away.Rain.IsSet() || Away.Cold.IsSet() || Away.Heat.IsSet()));

		Volume->Destroy();
		const FArcEnviroLevelOverrides After = Harness.Enviro->GetOverridesAtLocation(Inside);
		ASSERT_THAT(IsFalse(After.Rain.IsSet() || After.Cold.IsSet() || After.Heat.IsSet()));
	}

	TEST_METHOD(Volume_ReportsOnlyTheAxesItOverrides)
	{
		using namespace ArcEnviroVolumeTestsPrivate;

		AArcEnviroVolume* Volume = SpawnVolume(Spawner, FVector::ZeroVector, MakeSetup(Unset, 2, Unset, 0));
		ASSERT_THAT(IsNotNull(Volume));

		const FArcEnviroLevelOverrides Overrides = Harness.Enviro->GetOverridesAtLocation(Inside);
		ASSERT_THAT(IsFalse(Overrides.Rain.IsSet()));
		ASSERT_THAT(IsTrue(Overrides.Cold.IsSet() && Overrides.Cold.GetValue() == 2));
		ASSERT_THAT(IsFalse(Overrides.Heat.IsSet()));
	}

	TEST_METHOD(OverlappingVolumes_HighestPriorityWins_InAnySpawnOrder)
	{
		using namespace ArcEnviroVolumeTestsPrivate;

		for (int32 Order = 0; Order < 2; ++Order)
		{
			const FVolumeSetup Low = MakeSetup(Unset, 3, Unset, 1);
			const FVolumeSetup High = MakeSetup(Unset, 0, Unset, 5);
			AArcEnviroVolume* First = SpawnVolume(Spawner, FVector::ZeroVector, Order == 0 ? Low : High);
			AArcEnviroVolume* Second = SpawnVolume(Spawner, FVector::ZeroVector, Order == 0 ? High : Low);
			ASSERT_THAT(IsTrue(First != nullptr && Second != nullptr));

			const FArcEnviroLevelOverrides Overrides = Harness.Enviro->GetOverridesAtLocation(Inside);
			ASSERT_THAT(IsTrue(Overrides.Cold.IsSet() && Overrides.Cold.GetValue() == 0));

			First->Destroy();
			Second->Destroy();
		}
	}

	TEST_METHOD(OverlappingVolumes_EqualPriority_LowerLevelWins_InAnySpawnOrder)
	{
		using namespace ArcEnviroVolumeTestsPrivate;

		for (int32 Order = 0; Order < 2; ++Order)
		{
			const FVolumeSetup Shelter = MakeSetup(0, 1, Unset, 2);
			const FVolumeSetup Storm = MakeSetup(3, 3, Unset, 2);
			AArcEnviroVolume* First = SpawnVolume(Spawner, FVector::ZeroVector, Order == 0 ? Shelter : Storm);
			AArcEnviroVolume* Second = SpawnVolume(Spawner, FVector::ZeroVector, Order == 0 ? Storm : Shelter);
			ASSERT_THAT(IsTrue(First != nullptr && Second != nullptr));

			const FArcEnviroLevelOverrides Overrides = Harness.Enviro->GetOverridesAtLocation(Inside);
			ASSERT_THAT(IsTrue(Overrides.Rain.IsSet() && Overrides.Rain.GetValue() == 0));
			ASSERT_THAT(IsTrue(Overrides.Cold.IsSet() && Overrides.Cold.GetValue() == 1));

			First->Destroy();
			Second->Destroy();
		}
	}

	TEST_METHOD(OverlappingVolumes_HigherPriorityWater_BeatsShelterRain)
	{
		using namespace ArcEnviroVolumeTestsPrivate;

		AArcEnviroVolume* Shelter = SpawnVolume(Spawner, FVector::ZeroVector, MakeSetup(0, Unset, Unset, 1));
		AArcEnviroVolume* Water = SpawnVolume(Spawner, FVector::ZeroVector, MakeSetup(3, Unset, Unset, 5));
		ASSERT_THAT(IsTrue(Shelter != nullptr && Water != nullptr));

		const FArcEnviroLevelOverrides Overrides = Harness.Enviro->GetOverridesAtLocation(Inside);
		ASSERT_THAT(IsTrue(Overrides.Rain.IsSet() && Overrides.Rain.GetValue() == 3));
	}

	TEST_METHOD(OverlappingVolumes_HigherPriorityVolumeThatSkipsAnAxis_DoesNotMaskLowerOne)
	{
		using namespace ArcEnviroVolumeTestsPrivate;

		AArcEnviroVolume* High = SpawnVolume(Spawner, FVector::ZeroVector, MakeSetup(1, Unset, Unset, 10));
		AArcEnviroVolume* Low = SpawnVolume(Spawner, FVector::ZeroVector, MakeSetup(Unset, 2, Unset, 1));
		ASSERT_THAT(IsTrue(High != nullptr && Low != nullptr));

		const FArcEnviroLevelOverrides Overrides = Harness.Enviro->GetOverridesAtLocation(Inside);
		ASSERT_THAT(IsTrue(Overrides.Rain.IsSet() && Overrides.Rain.GetValue() == 1));
		ASSERT_THAT(IsTrue(Overrides.Cold.IsSet() && Overrides.Cold.GetValue() == 2));
		ASSERT_THAT(IsFalse(Overrides.Heat.IsSet()));
	}
};
