// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcMassQuickBarSharedFragment.generated.h"

class UArcMassQuickBarConfig;

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcMassQuickBarConfig> Config = nullptr;
};
