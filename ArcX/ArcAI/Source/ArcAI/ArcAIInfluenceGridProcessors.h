// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/ArcMassInfluenceMapping.h"
#include "UObject/Object.h"
#include "ArcAIInfluenceGridProcessors.generated.h"

/**
 * 
 */
UCLASS()
class ARCAI_API UArcAIInfluenceGridProcessors : public UObject
{
	GENERATED_BODY()
};

USTRUCT()
struct FArcInfluenceGridTag_Small : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class ARCAI_API UArcInfluenceMappingProcessor_Small : public UArcInfluenceMappingProcessor
{
	GENERATED_BODY()

public:
	UArcInfluenceMappingProcessor_Small();

protected:
	//virtual int32 GetGridIndex() const PURE_VIRTUAL(UArcInfluenceMappingProcessor::GetGridIndex, return 0;);
	virtual void AddGridTagRequirement(FMassEntityQuery& Query) override;
};