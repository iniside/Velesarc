// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcRmlDocumentRef.generated.h"

USTRUCT(BlueprintType)
struct ARCRMLUIDATA_API FArcRmlDocumentRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RmlUI")
	FString Path;

	bool IsSet() const
	{
		return !Path.IsEmpty();
	}

	bool operator==(const FArcRmlDocumentRef& Other) const
	{
		return Path == Other.Path;
	}

	bool operator!=(const FArcRmlDocumentRef& Other) const
	{
		return Path != Other.Path;
	}
};
