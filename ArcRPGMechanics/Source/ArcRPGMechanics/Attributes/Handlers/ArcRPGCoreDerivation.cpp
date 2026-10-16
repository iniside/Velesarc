// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/Handlers/ArcRPGCoreDerivation.h"

#include "DaySequenceActor.h"
#include "DaySequenceSubsystem.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"

namespace ArcRPGCoreDerivationDetails
{
	static float GetModifier(float Value)
	{
		return FMath::FloorToFloat((Value - 10.f) / 2.f);
	}
}

namespace ArcRPGCoreDerivation
{
	float CalculateModifier(float Value)
	{
		return ArcRPGCoreDerivationDetails::GetModifier(Value);
	}

	float ResolveRealSecondsPerGameHour(UWorld* World)
	{
		const float DefaultSecondsPerHour = 120.f;

		if (!World)
		{
			return DefaultSecondsPerHour;
		}

		UDaySequenceSubsystem* DaySub = World->GetSubsystem<UDaySequenceSubsystem>();
		if (!DaySub)
		{
			return DefaultSecondsPerHour;
		}

		ADaySequenceActor* DayActor = DaySub->GetDaySequenceActor();
		if (!DayActor)
		{
			return DefaultSecondsPerHour;
		}

		const float CycleRealHours = DayActor->GetTimePerCycle();
		const float PlayRate = FMath::Max(DayActor->GetPlayRate(), SMALL_NUMBER);

		if (CycleRealHours <= SMALL_NUMBER)
		{
			return DefaultSecondsPerHour;
		}

		return (CycleRealHours * 3600.f) / 24.f / PlayRate;
	}
} // namespace ArcRPGCoreDerivation
