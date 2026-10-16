// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "Engine/DataAsset.h"
#include "UObject/UnrealType.h"

namespace ArcTimeOfDayAstronomy
{
	struct FLunarArgument
	{
		int32 D;
		int32 M;
		int32 MP;
		int32 F;
	};

	struct FLunarCoefficient
	{
		int32 Longitude;
		int32 Distance;
	};

	constexpr FLunarArgument Lunar47AArguments[] =
	{
		{0,0,1,0},{2,0,-1,0},{2,0,0,0},{0,0,2,0},{0,1,0,0},{0,0,0,2},{2,0,-2,0},{2,-1,-1,0},{2,0,1,0},{2,-1,0,0},
		{0,1,-1,0},{1,0,0,0},{0,1,1,0},{2,0,0,-2},{0,0,1,2},{0,0,1,-2},{4,0,-1,0},{0,0,3,0},{4,0,-2,0},{2,1,-1,0},
		{2,1,0,0},{1,0,-1,0},{1,1,0,0},{2,-1,1,0},{2,0,2,0},{4,0,0,0},{2,0,-3,0},{0,1,-2,0},{2,0,-1,2},{2,-1,-2,0},
		{1,0,1,0},{2,-2,0,0},{0,1,2,0},{0,2,0,0},{2,-2,-1,0},{2,0,1,-2},{2,0,0,2},{4,-1,-1,0},{0,0,2,2},{3,0,-1,0},
		{2,1,1,0},{4,-1,-2,0},{0,2,-1,0},{2,2,-1,0},{2,1,-2,0},{2,-1,0,-2},{4,0,1,0},{0,0,4,0},{4,-1,0,0},{1,0,-2,0},
		{2,1,0,-2},{0,0,2,-2},{1,1,1,0},{3,0,-2,0},{4,0,-3,0},{2,-1,2,0},{0,2,1,0},{1,1,-1,0},{2,0,3,0},{2,0,-1,-2}
	};

	constexpr FLunarCoefficient Lunar47ACoefficients[] =
	{
		{6288774,-20905355},{1274027,-3699111},{658314,-2955968},{213618,-569925},{-185116,48888},{-114332,-3149},{58793,246158},{57066,-152138},{53322,-170733},{45758,-204586},
		{-40923,-129620},{-34720,108743},{-30383,104755},{15327,10321},{-12528,0},{10980,79661},{10675,-34782},{10034,-23210},{8548,-21636},{-7888,24208},
		{-6766,30824},{-5163,-8379},{4987,-16675},{4036,-12831},{3994,-10445},{3861,-11650},{3665,14403},{-2689,-7003},{-2602,0},{2390,10056},
		{-2348,6322},{2236,-9884},{-2120,5751},{-2069,0},{2048,-4950},{-1773,4130},{-1595,0},{1215,-3958},{-1110,0},{-892,3258},
		{-810,2616},{759,-1897},{-713,-2117},{-700,2354},{691,0},{596,0},{549,-1423},{537,-1117},{520,-1571},{-487,-1739},
		{-399,0},{-381,-4421},{351,0},{-340,0},{330,0},{327,0},{-323,1165},{299,0},{294,0},{0,8752}
	};

	constexpr FLunarArgument Lunar47BArguments[] =
	{
		{0,0,0,1},{0,0,1,1},{0,0,1,-1},{2,0,0,-1},{2,0,-1,1},{2,0,-1,-1},{2,0,0,1},{0,0,2,1},{2,0,1,-1},{0,0,2,-1},
		{2,-1,0,-1},{2,0,-2,-1},{2,0,1,1},{2,1,0,-1},{2,-1,-1,1},{2,-1,0,1},{2,-1,-1,-1},{0,1,-1,-1},{4,0,-1,-1},{0,1,0,1},
		{0,0,0,3},{0,1,-1,1},{1,0,0,1},{0,1,1,1},{0,1,1,-1},{0,1,0,-1},{1,0,0,-1},{0,0,3,1},{4,0,0,-1},{4,0,-1,1},
		{0,0,1,-3},{4,0,-2,1},{2,0,0,-3},{2,0,2,-1},{2,-1,1,-1},{2,0,-2,1},{0,0,3,-1},{2,0,2,1},{2,0,-3,-1},{2,1,-1,1},
		{2,1,0,1},{4,0,0,1},{2,-1,1,1},{2,-2,0,-1},{0,0,1,3},{2,1,1,-1},{1,1,0,-1},{1,1,0,1},{0,1,-2,-1},{2,1,-1,-1},
		{1,0,1,1},{2,-1,-2,-1},{0,1,2,1},{4,0,-2,-1},{4,-1,-1,-1},{1,0,1,-1},{4,0,1,-1},{1,0,-1,-1},{4,-1,0,-1},{2,-2,0,1}
	};

	constexpr int32 Lunar47BCoefficients[] =
	{
		5128122,280602,277693,173237,55413,46271,32573,17198,9266,8822,8216,4324,4200,-3359,2463,2211,2065,-1870,1828,-1794,
		-1749,-1565,-1491,-1475,-1410,-1344,-1335,1107,1021,833,777,671,607,596,491,-451,439,422,421,-366,-351,331,315,302,
		-283,-229,223,223,-220,-220,-185,181,-177,176,166,-164,132,-119,115,107
	};

	double SinDegrees(const double Degrees)
	{
		return FMath::Sin(FMath::DegreesToRadians(Degrees));
	}

	double CosDegrees(const double Degrees)
	{
		return FMath::Cos(FMath::DegreesToRadians(Degrees));
	}

	double TanDegrees(const double Degrees)
	{
		return FMath::Tan(FMath::DegreesToRadians(Degrees));
	}

	double AsinDegrees(const double Value)
	{
		return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Value, -1.0, 1.0)));
	}

	double Atan2Degrees(const double Y, const double X)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Y, X));
	}

	TArray<int32> MonthLengthsForYear(const FArcTimeOfDayCalendarData& CalendarData, const int32 Year)
	{
		TArray<int32> Result = CalendarData.MonthLengths;
		if (CalendarData.bUsesLeapYear && FDateTime::IsLeapYear(Year) && Result.IsValidIndex(CalendarData.LeapYearMonth - 1))
		{
			++Result[CalendarData.LeapYearMonth - 1];
		}
		return Result;
	}
}

double AArcTimeOfDayActor::WrapDegrees(const double Degrees)
{
	double Result = FMath::Fmod(Degrees, 360.0);
	return Result < 0.0 ? Result + 360.0 : Result;
}

bool AArcTimeOfDayActor::ReadCalendarData(FArcTimeOfDayCalendarData& OutCalendar) const
{
	if (!Calendar)
	{
		return false;
	}

	// Calendar data is read from the unchanged UDS asset without writing derived fields back.
	const UClass* CalendarClass = Calendar->GetClass();
	const FMapProperty* MonthsProperty = FindFProperty<FMapProperty>(CalendarClass, TEXT("Months"));
	const FArrayProperty* DaysProperty = FindFProperty<FArrayProperty>(CalendarClass, TEXT("Days of the Week"));
	const FBoolProperty* UsesLeapYearProperty = FindFProperty<FBoolProperty>(CalendarClass, TEXT("Uses Leap Year"));
	const FIntProperty* LeapYearMonthProperty = FindFProperty<FIntProperty>(CalendarClass, TEXT("Leap Year Month"));
	const FIntProperty* WinterSolsticeProperty = FindFProperty<FIntProperty>(CalendarClass, TEXT("Winter Solstice Offset"));
	const FIntProperty* DayOfWeekOffsetProperty = FindFProperty<FIntProperty>(CalendarClass, TEXT("Day of the Week Offset"));
	if (!MonthsProperty || !DaysProperty || !UsesLeapYearProperty || !LeapYearMonthProperty || !WinterSolsticeProperty || !DayOfWeekOffsetProperty)
	{
		return false;
	}

	const FStrProperty* MonthKeyProperty = CastField<FStrProperty>(MonthsProperty->KeyProp);
	const FIntProperty* MonthValueProperty = CastField<FIntProperty>(MonthsProperty->ValueProp);
	const FStrProperty* DayElementProperty = CastField<FStrProperty>(DaysProperty->Inner);
	if (!MonthKeyProperty || !MonthValueProperty || !DayElementProperty)
	{
		return false;
	}

	OutCalendar.MonthLengths.Reset();
	const void* MapAddress = MonthsProperty->ContainerPtrToValuePtr<void>(Calendar);
	FScriptMapHelper MonthMap(MonthsProperty, MapAddress);
	for (int32 Index = 0; Index < MonthMap.GetMaxIndex(); ++Index)
	{
		if (MonthMap.IsValidIndex(Index))
		{
			OutCalendar.MonthLengths.Add(MonthValueProperty->GetPropertyValue(MonthMap.GetValuePtr(Index)));
		}
	}

	OutCalendar.DaysOfWeek.Reset();
	const void* ArrayAddress = DaysProperty->ContainerPtrToValuePtr<void>(Calendar);
	FScriptArrayHelper DaysArray(DaysProperty, ArrayAddress);
	for (int32 Index = 0; Index < DaysArray.Num(); ++Index)
	{
		OutCalendar.DaysOfWeek.Add(DayElementProperty->GetPropertyValue(DaysArray.GetRawPtr(Index)));
	}

	OutCalendar.bUsesLeapYear = UsesLeapYearProperty->GetPropertyValue_InContainer(Calendar);
	OutCalendar.LeapYearMonth = LeapYearMonthProperty->GetPropertyValue_InContainer(Calendar);
	OutCalendar.WinterSolsticeOffset = WinterSolsticeProperty->GetPropertyValue_InContainer(Calendar);
	OutCalendar.DayOfWeekOffset = DayOfWeekOffsetProperty->GetPropertyValue_InContainer(Calendar);
	return OutCalendar.MonthLengths.Num() > 0 && OutCalendar.DaysOfWeek.Num() > 1;
}

bool AArcTimeOfDayActor::IsLeapYear(const int32 InYear, const FArcTimeOfDayCalendarData& CalendarData) const
{
	return CalendarData.bUsesLeapYear && FDateTime::IsLeapYear(InYear);
}

void AArcTimeOfDayActor::ForceValidDate(const FArcTimeOfDayCalendarData& CalendarData)
{
	if (Year == 0)
	{
		Year = 1;
	}
	Month = FMath::Clamp(Month, 1, CalendarData.MonthLengths.Num());
	const TArray<int32> MonthLengths = ArcTimeOfDayAstronomy::MonthLengthsForYear(CalendarData, Year);
	CalendarDay = FMath::Clamp(CalendarDay, 1, MonthLengths[Month - 1]);
}

void AArcTimeOfDayActor::OffsetDate(int32& InOutMonth, int32& InOutDay, int32& InOutYear, const int32 Offset, const FArcTimeOfDayCalendarData& CalendarData) const
{
	const int32 Direction = FMath::Sign(Offset);
	for (int32 Step = 0; Step < FMath::Abs(Offset); ++Step)
	{
		const TArray<int32> MonthLengths = ArcTimeOfDayAstronomy::MonthLengthsForYear(CalendarData, InOutYear);
		const int32 CandidateDay = InOutDay + Direction;
		if (CandidateDay >= 1 && CandidateDay <= MonthLengths[InOutMonth - 1])
		{
			InOutDay = CandidateDay;
			continue;
		}
		const int32 CandidateMonth = InOutMonth + Direction;
		if (CandidateMonth >= 1 && CandidateMonth <= MonthLengths.Num())
		{
			InOutMonth = CandidateMonth;
			InOutDay = Direction > 0 ? 1 : ArcTimeOfDayAstronomy::MonthLengthsForYear(CalendarData, InOutYear)[InOutMonth - 1];
			continue;
		}
		InOutYear += Direction;
		if (InOutYear == 0)
		{
			InOutYear = Direction;
		}
		InOutMonth = Direction > 0 ? 1 : MonthLengths.Num();
		InOutDay = Direction > 0 ? 1 : ArcTimeOfDayAstronomy::MonthLengthsForYear(CalendarData, InOutYear)[InOutMonth - 1];
	}
}

bool AArcTimeOfDayActor::IsDaylightSavingsTime(const FArcTimeOfDayCalendarData& CalendarData) const
{
	if (!bApplyDaylightSavingsTime || (!bSimulateRealSun && !bSimulateRealMoon && !bSimulateRealStars) || CalendarData.DaysOfWeek.Num() != 7 || Year < 1)
	{
		return false;
	}

	const int32 Months[2] = { DSTStartMonth, DSTEndMonth };
	const int32 Counts[2] = { DSTStartDayCount, DSTEndDayCount };
	const int32 Weekdays[2] = { DSTStartDayOfWeek, DSTEndDayOfWeek };
	const int32 Hours[2] = { DSTStartHour, DSTEndHour };
	float Boundaries[2] = { 0.0f, 0.0f };
	const TArray<int32> MonthLengths = ArcTimeOfDayAstronomy::MonthLengthsForYear(CalendarData, Year);
	for (int32 BoundaryIndex = 0; BoundaryIndex < 2; ++BoundaryIndex)
	{
		const int32 TestedMonth = FMath::Clamp(Months[BoundaryIndex], 1, MonthLengths.Num());
		const int32 FirstDayOfMonth = static_cast<int32>(FDateTime(Year, TestedMonth, 1).GetDayOfWeek());
		const int32 FirstInstance = FMath::FloorToInt(FMath::Fmod(static_cast<float>(Weekdays[BoundaryIndex] - FirstDayOfMonth + 7), 7.0f)) + 1;
		const int32 InstanceCount = Counts[BoundaryIndex] > 0 ? Counts[BoundaryIndex] : ((MonthLengths[TestedMonth - 1] - FirstInstance) / 7 + 1);
		const int32 BoundaryDay = FirstInstance + (InstanceCount - 1) * 7;
		int32 DaysBefore = 0;
		for (int32 MonthIndex = 0; MonthIndex < TestedMonth - 1; ++MonthIndex)
		{
			DaysBefore += MonthLengths[MonthIndex];
		}
		Boundaries[BoundaryIndex] = static_cast<float>(DaysBefore + BoundaryDay - 1) + static_cast<float>(Hours[BoundaryIndex]) / 24.0f;
	}

	int32 DaysBeforeCurrentMonth = 0;
	for (int32 MonthIndex = 0; MonthIndex < Month - 1; ++MonthIndex)
	{
		DaysBeforeCurrentMonth += MonthLengths[MonthIndex];
	}
	const float CurrentProgress = static_cast<float>(DaysBeforeCurrentMonth + CalendarDay - 1) + static_cast<float>(TimeOfDay / 2400.0);
	return Boundaries[0] < Boundaries[1]
		? CurrentProgress >= Boundaries[0] && CurrentProgress <= Boundaries[1]
		: !(CurrentProgress >= Boundaries[1] && CurrentProgress <= Boundaries[0]);
}

double AArcTimeOfDayActor::CalculateJulianDay(const double InDay, int32 InMonth, int32 InYear)
{
	if (InMonth <= 2)
	{
		--InYear;
		InMonth += 12;
	}
	const int32 A = FMath::FloorToInt(static_cast<double>(InYear) / 100.0);
	const int32 B = 2 - A + FMath::FloorToInt(static_cast<double>(A) / 4.0);
	return FMath::Floor(static_cast<double>(InYear + 4716) * 365.25)
		+ FMath::Floor(static_cast<double>(InMonth + 1) * 30.6001)
		+ InDay + static_cast<double>(B) - 1524.5;
}

void AArcTimeOfDayActor::UpdateAstronomicalSimulation(FArcTimeOfDayAstronomyResult& Result)
{
	using namespace ArcTimeOfDayAstronomy;

	FArcTimeOfDayCalendarData CalendarData;
	if (!ReadCalendarData(CalendarData))
	{
		return;
	}
	ForceValidDate(CalendarData);
	bDaylightSavingsActive = IsDaylightSavingsTime(CalendarData);

	double UTCTime = TimeOfDay - (bDaylightSavingsActive ? 100.0 : 0.0) - TimeZone * 100.0;
	int32 UTCMonth = Month;
	int32 UTCDay = CalendarDay;
	int32 UTCYear = Year;
	if (UTCTime < 0.0 || UTCTime >= 2400.0)
	{
		OffsetDate(UTCMonth, UTCDay, UTCYear, UTCTime >= 2400.0 ? 1 : -1, CalendarData);
	}
	UTCTime = FMath::Fmod(UTCTime + 2400.0, 2400.0);

	const double JulianDay = CalculateJulianDay(static_cast<double>(UTCDay) + UTCTime / 2400.0, UTCMonth, UTCYear);
	const double T = (JulianDay - 2451545.0) / 36525.0;
	const double T2 = FMath::Pow(T, 2.0);
	const double T3 = FMath::Pow(T, 3.0);
	const double T4 = FMath::Pow(T, 4.0);
	const FVector CelestialNorthPole = FRotator(0.0, NorthYaw + 90.0, 0.0).RotateVector(FRotator(Latitude - 90.0, 0.0, 0.0).RotateVector(FVector::UpVector)).GetSafeNormal();

	const double SunGeometricMeanLongitude = 280.46646 + 36000.76983 * T;
	const double SunMeanAnomaly = 357.52911 + 35999.05029 * T - 0.0001537 * T2;
	const double SolarEccentricity = 0.016708634 - 0.000042037 * T - 0.0000001267 * T2;
	const double SunEquationOfCenter = (1.914602 - 0.004817 * T - 0.000014 * T2) * SinDegrees(SunMeanAnomaly)
		+ (0.019993 - 0.000101 * T) * SinDegrees(2.0 * SunMeanAnomaly)
		+ 0.000289 * SinDegrees(3.0 * SunMeanAnomaly);
	const double SunTrueLongitude = SunGeometricMeanLongitude + SunEquationOfCenter;
	const double ObliquityCorrectionAngle = 125.04 - 1934.136 * T;
	const double SunApparentLongitude = SunTrueLongitude - 0.00569 - 0.00478 * SinDegrees(ObliquityCorrectionAngle);
	const double ObliquityOfEcliptic = 23.439291 - 0.013004 * T - 0.00000016 * T2 + 0.0000005 * T3;
	const double CorrectedObliquity = ObliquityOfEcliptic + 0.00256 * CosDegrees(ObliquityCorrectionAngle);
	const double SunRightAscension = Atan2Degrees(SinDegrees(SunApparentLongitude) * CosDegrees(CorrectedObliquity), CosDegrees(SunApparentLongitude));
	const double SunDeclination = AsinDegrees(SinDegrees(SunApparentLongitude) * SinDegrees(CorrectedObliquity));
	const double GreenwichSiderealTime = WrapDegrees(280.46061837 + (JulianDay - 2451545.0) * 360.98564736629 + 0.000387933 * T2 - T3 / 38710000.0);
	const double SunLocalHourAngle = GreenwichSiderealTime + Longitude - SunRightAscension;
	const double SunAzimuth = Atan2Degrees(SinDegrees(SunLocalHourAngle), CosDegrees(SunLocalHourAngle) * SinDegrees(Latitude) - TanDegrees(SunDeclination) * CosDegrees(Latitude));
	const double SunAltitude = AsinDegrees(SinDegrees(Latitude) * SinDegrees(SunDeclination) + CosDegrees(Latitude) * CosDegrees(SunDeclination) * CosDegrees(SunLocalHourAngle));
	Result.SunDirection = FRotator(0.0, SunAzimuth + 180.0, 0.0).RotateVector(FRotator(SunAltitude, 0.0, 0.0).RotateVector(-FVector::ForwardVector)).GetSafeNormal();
	if (bSimulateRealSun && bSimulationControlsSunScale)
	{
		const double SunTrueAnomaly = SunMeanAnomaly + SunEquationOfCenter;
		const double SunAUDistance = (1.000001018 * (1.0 - SolarEccentricity * SolarEccentricity)) / (1.0 + SolarEccentricity * CosDegrees(SunTrueAnomaly));
		SunScale = 0.5334 / SunAUDistance;
	}

	if (bSimulateRealMoon)
	{
		const double MoonMeanLongitude = WrapDegrees(218.3164477 + 481267.88123421 * T - 0.0015786 * T2 + T3 / 538841.0 - T4 / 65194000.0);
		const double MoonMeanElongation = WrapDegrees(297.8501921 + 445267.1114034 * T - 0.0018819 * T2 + T3 / 545868.0 - T4 / 113065000.0);
		const double MoonSolarMeanAnomaly = WrapDegrees(357.5291092 + 35999.0502909 * T - 0.0001536 * T2 + T3 / 24490000.0);
		const double MoonMeanAnomaly = WrapDegrees(134.9633964 + 477198.8675055 * T + 0.0087414 * T2 + T3 / 69699.0 - T4 / 14712000.0);
		const double MoonArgumentOfLatitude = WrapDegrees(93.272095 + 483202.0175233 * T - 0.0036539 * T2 - T3 / 3526000.0 + T4 / 863310000.0);
		const double LunarEccentricity = 1.0 - 0.002516 * T - 0.0000074 * T2;
		const double Additive1 = WrapDegrees(119.75 + 131.849 * T);
		const double Additive2 = WrapDegrees(53.09 + 479264.29 * T);
		const double Additive3 = WrapDegrees(313.45 + 481266.484 * T);
		double LongitudeTerms = 0.0;
		double DistanceTerms = 0.0;
		double LatitudeTerms = 0.0;
		const int32 Terms = FMath::Clamp(MoonSimulationTerms, 1, 60);
		for (int32 Index = 0; Index < Terms; ++Index)
		{
			const FLunarArgument& A = Lunar47AArguments[Index];
			const double Argument = A.D * MoonMeanElongation + A.M * MoonSolarMeanAnomaly + A.MP * MoonMeanAnomaly + A.F * MoonArgumentOfLatitude;
			const double EccentricityFactor = FMath::Pow(LunarEccentricity, FMath::Abs(A.M));
			LongitudeTerms += static_cast<double>(Lunar47ACoefficients[Index].Longitude) * SinDegrees(Argument) * EccentricityFactor;
			DistanceTerms += static_cast<double>(Lunar47ACoefficients[Index].Distance) * CosDegrees(Argument) * EccentricityFactor;
			const FLunarArgument& B = Lunar47BArguments[Index];
			const double LatitudeArgument = B.D * MoonMeanElongation + B.M * MoonSolarMeanAnomaly + B.MP * MoonMeanAnomaly + B.F * MoonArgumentOfLatitude;
			LatitudeTerms += static_cast<double>(Lunar47BCoefficients[Index]) * SinDegrees(LatitudeArgument) * FMath::Pow(LunarEccentricity, FMath::Abs(B.M));
		}
		LongitudeTerms += 3958.0 * SinDegrees(Additive1)
			+ 1962.0 * SinDegrees(MoonMeanLongitude - MoonArgumentOfLatitude)
			+ 318.0 * SinDegrees(Additive2);
		LatitudeTerms += -2235.0 * SinDegrees(MoonMeanLongitude)
			+ 382.0 * SinDegrees(Additive3)
			+ 175.0 * SinDegrees(Additive1 - MoonArgumentOfLatitude)
			+ 175.0 * SinDegrees(Additive1 + MoonArgumentOfLatitude)
			+ 127.0 * SinDegrees(MoonMeanLongitude - MoonMeanAnomaly)
			- 115.0 * SinDegrees(MoonMeanLongitude + MoonMeanAnomaly);
		const double MoonGeocentricLongitude = MoonMeanLongitude + LongitudeTerms / 1000000.0;
		const double MoonGeocentricLatitude = LatitudeTerms / 1000000.0;
		Result.MoonDistance = 385000.56 + DistanceTerms / 1000.0;
		const double EquatorialHorizontalParallax = AsinDegrees(6378.14 / Result.MoonDistance);
		const double AscendingNodeLongitude = 125.04452 - 1934.136261 * T + 0.0020708 * T2 + T3 / 450000.0;
		const double LongitudeNutation = -17.1996 * SinDegrees(AscendingNodeLongitude) - 1.3187 * SinDegrees(2.0 * MoonMeanElongation - 2.0 * MoonArgumentOfLatitude + 2.0 * AscendingNodeLongitude);
		const double MoonApparentLongitude = MoonGeocentricLongitude + LongitudeNutation / 3600.0;
		const double MoonGeocentricRightAscension = Atan2Degrees(SinDegrees(MoonApparentLongitude) * CosDegrees(CorrectedObliquity) - TanDegrees(MoonGeocentricLatitude) * SinDegrees(CorrectedObliquity), CosDegrees(MoonApparentLongitude));
		const double MoonGeocentricDeclination = AsinDegrees(SinDegrees(MoonGeocentricLatitude) * CosDegrees(CorrectedObliquity) + CosDegrees(MoonGeocentricLatitude) * SinDegrees(CorrectedObliquity) * SinDegrees(MoonApparentLongitude));
		const double MoonLocalHourAngle = GreenwichSiderealTime + Longitude - MoonGeocentricRightAscension;
		const double ParallaxShift = Atan2Degrees(-CosDegrees(Latitude) * SinDegrees(EquatorialHorizontalParallax) * SinDegrees(MoonLocalHourAngle), CosDegrees(MoonGeocentricDeclination) - CosDegrees(Latitude) * SinDegrees(EquatorialHorizontalParallax) * CosDegrees(MoonLocalHourAngle));
		const double MoonTopocentricRightAscension = MoonGeocentricRightAscension + ParallaxShift;
		const double TopocentricHourAngle = GreenwichSiderealTime + Longitude - MoonTopocentricRightAscension;
		const double MoonTopocentricDeclination = Atan2Degrees((SinDegrees(MoonGeocentricDeclination) - SinDegrees(Latitude) * SinDegrees(EquatorialHorizontalParallax)) * CosDegrees(ParallaxShift), CosDegrees(MoonGeocentricDeclination) - CosDegrees(Latitude) * SinDegrees(EquatorialHorizontalParallax) * CosDegrees(MoonLocalHourAngle));
		const double MoonAzimuth = Atan2Degrees(SinDegrees(TopocentricHourAngle), CosDegrees(TopocentricHourAngle) * SinDegrees(Latitude) - TanDegrees(MoonTopocentricDeclination) * CosDegrees(Latitude));
		const double MoonAltitude = AsinDegrees(SinDegrees(Latitude) * SinDegrees(MoonTopocentricDeclination) + CosDegrees(Latitude) * CosDegrees(MoonTopocentricDeclination) * CosDegrees(TopocentricHourAngle));
		Result.MoonDirection = FRotator(0.0, MoonAzimuth + 180.0, 0.0).RotateVector(FRotator(MoonAltitude, 0.0, 0.0).RotateVector(-FVector::ForwardVector)).GetSafeNormal();

		const FMatrix MoonTextureSpace = FRotationMatrix::MakeFromXY(Result.MoonDirection, CelestialNorthPole);
		Result.MoonAlignment = MoonTextureSpace.GetUnitAxis(EAxis::Z).GetSafeNormal();
		const FVector MoonToSun = (Result.MoonDirection * Result.MoonDistance - Result.SunDirection * 149000000.0).GetSafeNormal();
		const FVector LocalMoonToSun = MoonTextureSpace.InverseTransformVector(MoonToSun);
		Result.MoonPhaseAngle = FVector(-LocalMoonToSun.Z, -LocalMoonToSun.X, LocalMoonToSun.Y);
		MoonPhase = MapRangeClamped(static_cast<float>(WrapDegrees(Result.MoonPhaseAngle.Rotation().Yaw + 450.0)), 0.0f, 360.0f, 0.0f, 29.53f);
		if (bSimulationControlsMoonScale)
		{
			MoonScale = 3474.8 / FVector::Distance(Result.MoonDirection * Result.MoonDistance, FVector(0.0, 0.0, 6371.0)) * 57.296;
		}
	}

	if (bSimulateRealStars)
	{
		const FVector StarsYaw = FRotator(0.0, NorthYaw + GetActorRotation().Yaw, 0.0).RotateVector(FVector::RightVector);
		Result.StarsOrientation = FLinearColor(StarsYaw.X, StarsYaw.Y, StarsYaw.Z, -FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(FVector::UpVector, CelestialNorthPole))) / 360.0f);
		Result.StarsOrbit = static_cast<float>((-(NorthYaw + GetActorRotation().Yaw) - 360.0 * FMath::Frac(T * 100.0) - 360.0 * FMath::Frac(JulianDay) + 169.65 - Longitude) / 360.0);
	}
}
