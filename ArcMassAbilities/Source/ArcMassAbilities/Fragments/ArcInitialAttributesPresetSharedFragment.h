// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcInitialAttributesPresetSharedFragment.generated.h"

class UArcInitialAttributesPreset;

USTRUCT()
struct ARCMASSABILITIES_API FArcInitialAttributesPresetSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcInitialAttributesPreset> Preset = nullptr;
};
