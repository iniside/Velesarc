// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ArcMassEditorDebugDrawSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings)
class UArcMassEditorDebugDrawSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	TMap<FName, bool> CategoryToggleState;
};
