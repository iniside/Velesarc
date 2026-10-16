// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

float AArcTimeOfDayActor::MapRangeClamped(const float Value, const float InMin, const float InMax, const float OutMin, const float OutMax)
{
	if (FMath::IsNearlyEqual(InMin, InMax))
	{
		return OutMin;
	}
	const float Alpha = FMath::Clamp((Value - InMin) / (InMax - InMin), 0.0f, 1.0f);
	return FMath::Lerp(OutMin, OutMax, Alpha);
}

FLinearColor AArcTimeOfDayActor::Desaturate(const FLinearColor& Color, const float Fraction)
{
	return Color.Desaturate(Fraction);
}

FVector AArcTimeOfDayActor::RotateAroundAxis(const FVector& Vector, const FVector& Axis, const float Degrees)
{
	return Vector.RotateAngleAxis(Degrees, Axis.GetSafeNormal());
}

void AArcTimeOfDayActor::SetCloudScalar(const FName Name, const float Value)
{
	if (UWorld* World = GetWorld())
	{
		if (VolumetricCloudParameters)
		{
			if (UMaterialParameterCollectionInstance* Parameters = World->GetParameterCollectionInstance(VolumetricCloudParameters))
			{
				Parameters->SetScalarParameterValue(Name, Value);
			}
		}
	}
}

void AArcTimeOfDayActor::SetCloudVector(const FName Name, const FLinearColor& Value)
{
	if (UWorld* World = GetWorld())
	{
		if (VolumetricCloudParameters)
		{
			if (UMaterialParameterCollectionInstance* Parameters = World->GetParameterCollectionInstance(VolumetricCloudParameters))
			{
				Parameters->SetVectorParameterValue(Name, Value);
			}
		}
	}
}

void AArcTimeOfDayActor::SetWeatherScalar(const FName Name, const float Value)
{
	if (UWorld* World = GetWorld())
	{
		if (WeatherParameters)
		{
			if (UMaterialParameterCollectionInstance* Parameters = World->GetParameterCollectionInstance(WeatherParameters))
			{
				Parameters->SetScalarParameterValue(Name, Value);
			}
		}
	}
}

void AArcTimeOfDayActor::SetWeatherVector(const FName Name, const FLinearColor& Value)
{
	if (UWorld* World = GetWorld())
	{
		if (WeatherParameters)
		{
			if (UMaterialParameterCollectionInstance* Parameters = World->GetParameterCollectionInstance(WeatherParameters))
			{
				Parameters->SetVectorParameterValue(Name, Value);
			}
		}
	}
}
