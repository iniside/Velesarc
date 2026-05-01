// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Attributes/ArcAttribute.h"
#include "ArcAttributeCapture.generated.h"

UENUM(BlueprintType)
enum class EArcCaptureSource : uint8
{
	Source,
	Target
};

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcAttributeCaptureDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere)
	EArcCaptureSource CaptureSource = EArcCaptureSource::Source;

	UPROPERTY(EditAnywhere)
	bool bSnapshot = true;

	bool operator==(const FArcAttributeCaptureDef& Other) const;
	friend uint32 GetTypeHash(const FArcAttributeCaptureDef& Def);
};

struct ARCMASSABILITIES_API FArcCapturedAttribute
{
	FArcAttributeCaptureDef Definition;
	float SnapshotValue = 0.f;
	bool bHasSnapshot = false;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEvaluateParameters
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTagContainer SourceTags;

	UPROPERTY()
	FGameplayTagContainer TargetTags;
};
