// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAggregator.h"
#include "Mass/EntityHandle.h"
#include "ArcAttributeHandlerConfig.generated.h"

struct FMassExecutionContext;

class UArcAttributeHandlerConfig;

struct ARCMASSABILITIES_API FArcAttributeHandlerContext
{
	FMassEntityManager& EntityManager;
	FMassExecutionContext* MassContext;
	UWorld* World;
	FMassEntityHandle Entity;
	uint8* OwningFragmentMemory;
	const UArcAttributeHandlerConfig* HandlerConfig;

	explicit FArcAttributeHandlerContext(
		FMassEntityManager& InEntityManager,
		FMassExecutionContext* InMassContext,
		FMassEntityHandle InEntity,
		uint8* InOwningFragmentMemory,
		const UArcAttributeHandlerConfig* InHandlerConfig);

	FArcAttribute* ResolveAttribute(const FArcAttributeRef& AttrRef) const;
	bool ModifyInline(const FArcAttributeRef& AttrRef, float NewValue);
	float EvaluateWithTags(const FArcAttributeRef& AttrRef, float BaseValue,
	                       const FGameplayTagContainer& SourceTags,
	                       const FGameplayTagContainer& TargetTags) const;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAttributeHandler
{
	GENERATED_BODY()

	virtual ~FArcAttributeHandler() = default;

	virtual bool PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue) { return true; }
	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) {}
	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) {}
};

UCLASS(BlueprintType)
class ARCMASSABILITIES_API UArcAttributeHandlerConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Handlers", meta=(BaseStruct="/Script/ArcMassAbilities.ArcAttributeHandler", ExcludeBaseStruct))
	TMap<FArcAttributeRef, FInstancedStruct> Handlers;

	UPROPERTY(EditAnywhere, Category = "Aggregation", meta=(BaseStruct="/Script/ArcMassAbilities.ArcAggregationPolicy", ExcludeBaseStruct))
	TMap<FArcAttributeRef, FInstancedStruct> AggregationPolicies;

	const FArcAttributeHandler* FindHandler(const FArcAttributeRef& AttributeRef) const;
	const FInstancedStruct* FindAggregationPolicy(const FArcAttributeRef& AttributeRef) const;
};
