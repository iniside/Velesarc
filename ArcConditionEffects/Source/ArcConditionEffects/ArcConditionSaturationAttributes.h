// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"

#include "ArcConditionSaturationAttributes.generated.h"

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcConditionSaturationAttributes : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Thermal")
	FArcAttribute BurningSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Thermal")
	FArcAttribute ChilledSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Thermal")
	FArcAttribute OiledSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Thermal")
	FArcAttribute WetSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Biological")
	FArcAttribute BleedingSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Biological")
	FArcAttribute PoisonedSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Biological")
	FArcAttribute DiseasedSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Biological")
	FArcAttribute WeakenedSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Environmental")
	FArcAttribute ShockedSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Environmental")
	FArcAttribute CorrodedSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Environmental")
	FArcAttribute BlindedSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Environmental")
	FArcAttribute SuffocatingSaturation;

	UPROPERTY(VisibleAnywhere, Category = "Conditions|Environmental")
	FArcAttribute ExhaustedSaturation;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, BurningSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, ChilledSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, OiledSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, WetSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, BleedingSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, PoisonedSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, DiseasedSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, WeakenedSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, ShockedSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, CorrodedSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, BlindedSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, SuffocatingSaturation)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcConditionSaturationAttributes, ExhaustedSaturation)

	FArcAttribute* GetSaturationByIndex(int32 Index)
	{
		static const uint32 Offsets[] = {
			STRUCT_OFFSET(FArcConditionSaturationAttributes, BurningSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, BleedingSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, ChilledSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, ShockedSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, PoisonedSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, DiseasedSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, WeakenedSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, OiledSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, WetSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, CorrodedSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, BlindedSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, SuffocatingSaturation),
			STRUCT_OFFSET(FArcConditionSaturationAttributes, ExhaustedSaturation),
		};
		return reinterpret_cast<FArcAttribute*>(reinterpret_cast<uint8*>(this) + Offsets[Index]);
	}
};
