// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcKnowledgePayload.h"
#include "ArcKnowledgeTestPayload.generated.h"

/** Test payload for verifying PayloadType filter and FInstancedStruct round-tripping. */
USTRUCT()
struct FArcKnowledgeTestPayload : public FArcKnowledgePayload
{
	GENERATED_BODY()

	UPROPERTY()
	int32 TestValue = 0;
};
