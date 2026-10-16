// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcEnviroEffectsTestHarness.h"
#include "ArcWeather/ArcWeatherSubsystem.h"
#include "Components/ActorTestSpawner.h"

namespace ArcEnviroWeatherLevelTestsPrivate
{
	struct FLevelCase
	{
		float Value;
		int32 Level;
	};

	const FVector Origin = FVector::ZeroVector;
}

TEST_CLASS(ArcEnviroWeatherLevels, "ArcEnviroEffects.Weather")
{
	FActorTestSpawner Spawner;
	ArcEnviroTest::FHarness Harness;
	UArcWeatherSubsystem* Weather = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		const bool bInitialized = Harness.Initialize(Spawner);
		check(bInitialized);
		Weather = Harness.Weather;
	}

	TEST_METHOD(ColdLevel_FollowsConfiguredThresholds)
	{
		const ArcEnviroWeatherLevelTestsPrivate::FLevelCase Cases[] = {
			{22.f, 0}, {15.01f, 0}, {15.f, 1}, {10.f, 1}, {5.01f, 1}, {5.f, 2}, {0.f, 2}, {-4.99f, 2}, {-5.f, 3}, {-30.f, 3}
		};

		for (const ArcEnviroWeatherLevelTestsPrivate::FLevelCase& Case : Cases)
		{
			Weather->DefaultClimate.Temperature = Case.Value;
			ASSERT_THAT(AreEqual(Case.Level, Weather->GetColdLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
		}
	}

	TEST_METHOD(HeatLevel_FollowsConfiguredThresholds)
	{
		const ArcEnviroWeatherLevelTestsPrivate::FLevelCase Cases[] = {
			{22.f, 0}, {29.99f, 0}, {30.f, 1}, {37.99f, 1}, {38.f, 2}, {45.99f, 2}, {46.f, 3}, {60.f, 3}
		};

		for (const ArcEnviroWeatherLevelTestsPrivate::FLevelCase& Case : Cases)
		{
			Weather->DefaultClimate.Temperature = Case.Value;
			ASSERT_THAT(AreEqual(Case.Level, Weather->GetHeatLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
		}
	}

	TEST_METHOD(RainLevel_FollowsDefaultHumidityThresholdPlusOffsets)
	{
		const ArcEnviroWeatherLevelTestsPrivate::FLevelCase Cases[] = {
			{70.f, 0}, {74.99f, 0}, {75.f, 1}, {84.99f, 1}, {85.f, 2}, {94.99f, 2}, {95.f, 3}, {100.f, 3}
		};

		Weather->DefaultClimate.Temperature = ArcEnviroTest::NeutralTemperature;
		Weather->DefaultFreezeThreshold = 0.f;
		for (const ArcEnviroWeatherLevelTestsPrivate::FLevelCase& Case : Cases)
		{
			Weather->DefaultClimate.Humidity = Case.Value;
			ASSERT_THAT(AreEqual(Case.Level, Weather->GetRainLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
		}
	}

	TEST_METHOD(RainLevel_UsesCellHumidityThreshold)
	{
		FArcClimateParams Climate;
		Climate.Temperature = ArcEnviroTest::NeutralTemperature;
		Climate.Humidity = 44.f;
		Weather->SetBaseClimate(Weather->WorldToCell(ArcEnviroWeatherLevelTestsPrivate::Origin), Climate, 40.f, 0.f);
		ASSERT_THAT(AreEqual(0, Weather->GetRainLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));

		const ArcEnviroWeatherLevelTestsPrivate::FLevelCase Cases[] = {
			{45.f, 1}, {54.99f, 1}, {55.f, 2}, {64.99f, 2}, {65.f, 3}
		};
		for (const ArcEnviroWeatherLevelTestsPrivate::FLevelCase& Case : Cases)
		{
			Climate.Humidity = Case.Value;
			Weather->SetBaseClimate(Weather->WorldToCell(ArcEnviroWeatherLevelTestsPrivate::Origin), Climate, 40.f, 0.f);
			ASSERT_THAT(AreEqual(Case.Level, Weather->GetRainLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
		}
	}

	TEST_METHOD(RainLevel_IsZeroWhileFreezing)
	{
		FArcClimateParams Climate;
		Climate.Temperature = -5.f;
		Climate.Humidity = 100.f;
		const FIntVector Cell = Weather->WorldToCell(ArcEnviroWeatherLevelTestsPrivate::Origin);

		Weather->SetBaseClimate(Cell, Climate, ArcEnviroTest::CellHumidityThreshold, 0.f);
		ASSERT_THAT(AreEqual(0, Weather->GetRainLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
		ASSERT_THAT(AreEqual(3, Weather->GetColdLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));

		Climate.Temperature = 1.f;
		Weather->SetBaseClimate(Cell, Climate, ArcEnviroTest::CellHumidityThreshold, 0.f);
		ASSERT_THAT(AreEqual(3, Weather->GetRainLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));

		Climate.Temperature = -5.f;
		Weather->SetBaseClimate(Cell, Climate, ArcEnviroTest::CellHumidityThreshold, -10.f);
		ASSERT_THAT(AreEqual(3, Weather->GetRainLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
	}

	TEST_METHOD(ColdAndHeatLevels_AreNeverBothNonZero)
	{
		for (int32 Temperature = -40; Temperature <= 70; ++Temperature)
		{
			Weather->DefaultClimate.Temperature = static_cast<float>(Temperature);
			const int32 Cold = Weather->GetColdLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin);
			const int32 Heat = Weather->GetHeatLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin);
			ASSERT_THAT(IsTrue(Cold == 0 || Heat == 0));
			ASSERT_THAT(IsTrue(Cold >= 0 && Cold <= 3 && Heat >= 0 && Heat <= 3));
		}
	}

	TEST_METHOD(Levels_FollowWeatherOffsetOnTopOfBaseClimate)
	{
		FArcClimateParams Climate;
		Climate.Temperature = ArcEnviroTest::NeutralTemperature;
		Climate.Humidity = ArcEnviroTest::DryHumidity;
		Weather->SetBaseClimate(Weather->WorldToCell(ArcEnviroWeatherLevelTestsPrivate::Origin), Climate, ArcEnviroTest::CellHumidityThreshold, 0.f);
		ASSERT_THAT(AreEqual(0, Weather->GetColdLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
		ASSERT_THAT(AreEqual(0, Weather->GetRainLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));

		Weather->SetWeatherOffsetAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin, -17.f, 40.f);
		ASSERT_THAT(AreEqual(2, Weather->GetColdLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
		ASSERT_THAT(AreEqual(1, Weather->GetRainLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));

		Weather->SetWeatherOffsetAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin, 20.f, 0.f);
		ASSERT_THAT(AreEqual(2, Weather->GetHeatLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));

		Weather->ClearWeatherOffsetAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin);
		ASSERT_THAT(AreEqual(0, Weather->GetHeatLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
		ASSERT_THAT(AreEqual(0, Weather->GetColdLevelAtLocation(ArcEnviroWeatherLevelTestsPrivate::Origin)));
	}

	TEST_METHOD(Levels_AreIndependentPerCell)
	{
		FArcClimateParams Cold;
		Cold.Temperature = -10.f;
		Cold.Humidity = ArcEnviroTest::DryHumidity;
		FArcClimateParams Hot;
		Hot.Temperature = 50.f;
		Hot.Humidity = ArcEnviroTest::DryHumidity;

		const FVector ColdLocation(Weather->CellSize * 0.5f, 0.f, 0.f);
		const FVector HotLocation(Weather->CellSize * 1.5f, 0.f, 0.f);
		Weather->SetBaseClimate(Weather->WorldToCell(ColdLocation), Cold, ArcEnviroTest::CellHumidityThreshold, 0.f);
		Weather->SetBaseClimate(Weather->WorldToCell(HotLocation), Hot, ArcEnviroTest::CellHumidityThreshold, 0.f);

		ASSERT_THAT(AreEqual(3, Weather->GetColdLevelAtLocation(ColdLocation)));
		ASSERT_THAT(AreEqual(0, Weather->GetHeatLevelAtLocation(ColdLocation)));
		ASSERT_THAT(AreEqual(0, Weather->GetColdLevelAtLocation(HotLocation)));
		ASSERT_THAT(AreEqual(3, Weather->GetHeatLevelAtLocation(HotLocation)));
	}
};
