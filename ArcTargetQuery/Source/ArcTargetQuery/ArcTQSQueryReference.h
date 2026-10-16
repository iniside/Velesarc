// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "ArcTQSQueryReference.generated.h"

class UArcTQSQueryDefinition;

USTRUCT(BlueprintType)
struct ARCTARGETQUERY_API FArcTQSQueryReference
{
	GENERATED_BODY()

	bool IsValid() const
	{
		return Definition != nullptr;
	}

	const UArcTQSQueryDefinition* GetDefinition() const
	{
		return Definition;
	}

	void SetDefinition(UArcTQSQueryDefinition* NewDefinition);

	const FInstancedPropertyBag& GetParameters() const
	{
		return Parameters;
	}

	FInstancedPropertyBag& GetMutableParameters()
	{
		return Parameters;
	}

	bool IsPropertyOverridden(const FGuid PropertyID) const
	{
		return PropertyOverrides.Contains(PropertyID);
	}

	void SetPropertyOverridden(const FGuid PropertyID, bool bIsOverridden);

	void SyncParameters();

	bool HasMatchingLayout() const;

protected:
	UPROPERTY(EditAnywhere, Category = "Query")
	TObjectPtr<UArcTQSQueryDefinition> Definition = nullptr;

	UPROPERTY(EditAnywhere, Category = "Query", meta = (FixedLayout))
	FInstancedPropertyBag Parameters;

	UPROPERTY(EditAnywhere, Category = "Query")
	TArray<FGuid> PropertyOverrides;

	friend class FArcTQSQueryReferenceDetails;
};
