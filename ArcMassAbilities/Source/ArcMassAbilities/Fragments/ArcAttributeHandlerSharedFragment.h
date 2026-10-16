// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcAttributeHandlerSharedFragment.generated.h"

class UArcAttributeHandlerConfig;

USTRUCT()
struct ARCMASSABILITIES_API FArcAttributeHandlerSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcAttributeHandlerConfig> Config = nullptr;
};
