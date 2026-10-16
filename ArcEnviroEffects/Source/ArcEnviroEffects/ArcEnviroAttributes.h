// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcEnviroAttributes.generated.h"

USTRUCT()
struct ARCENVIROEFFECTS_API FArcEnviroAttributes : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Protection")
	FArcAttribute ColdProtection;

	UPROPERTY(EditAnywhere, Category = "Protection")
	FArcAttribute HeatProtection;

	UPROPERTY(EditAnywhere, Category = "Protection")
	FArcAttribute RainProtection;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcAttribute Wet;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcAttribute Temperature;

	UPROPERTY(EditAnywhere, Category = "Level", meta = (ToolTip = "Wet level 0..3, written by the condition-delta handler on WetDelta from the Wet value."))
	FArcAttribute WetLevel;

	UPROPERTY(EditAnywhere, Category = "Level", meta = (ToolTip = "Chilled level 0..3, written by the condition-delta handler on TemperatureDelta from the negated Temperature value."))
	FArcAttribute ChilledLevel;

	UPROPERTY(EditAnywhere, Category = "Level", meta = (ToolTip = "Hot level 0..3, written by the condition-delta handler on TemperatureDelta from the Temperature value."))
	FArcAttribute HotLevel;

	UPROPERTY(EditAnywhere, Category = "Meta")
	FArcAttribute WetDelta;

	UPROPERTY(EditAnywhere, Category = "Meta")
	FArcAttribute TemperatureDelta;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, ColdProtection)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, HeatProtection)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, RainProtection)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, Wet)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, Temperature)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, WetLevel)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, ChilledLevel)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, HotLevel)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, WetDelta)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcEnviroAttributes, TemperatureDelta)
};

template <>
struct TMassFragmentTraits<FArcEnviroAttributes> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
