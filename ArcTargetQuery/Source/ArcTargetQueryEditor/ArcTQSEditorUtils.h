// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcTQSQueryDefinition;
struct FPropertyBagPropertyDesc;

namespace ArcTQSEditor
{
	void CollectEntityParameters(const UArcTQSQueryDefinition* Definition, TArray<const FPropertyBagPropertyDesc*>& OutParameters);

	void NotifyPreviewSetupChanged(UArcTQSQueryDefinition& Definition);
}
