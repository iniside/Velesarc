// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcMassEntityConfigAsset;
class UBlueprint;

UArcMassEntityConfigAsset* ConvertBlueprintToEntityConfig(
	const UBlueprint& Blueprint,
	TArray<FText>& OutProblems);
