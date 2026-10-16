// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
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
	FArcAttributeRef Attribute;
	FMassEntityHandle SourceEntity;
	FGameplayTagContainer CapturedSourceTags;
	FGameplayTagContainer CapturedTargetTags;
	uint8* OwningFragmentMemory;
	const UArcAttributeHandlerConfig* HandlerConfig;

	explicit FArcAttributeHandlerContext(
		FMassEntityManager& InEntityManager,
		FMassExecutionContext* InMassContext,
		FMassEntityHandle InEntity,
		uint8* InOwningFragmentMemory,
		const UArcAttributeHandlerConfig* InHandlerConfig,
		const FArcAttributeRef& InAttribute,
		FMassEntityHandle InSourceEntity = FMassEntityHandle(),
		const FGameplayTagContainer& InCapturedSourceTags = FGameplayTagContainer(),
		const FGameplayTagContainer& InCapturedTargetTags = FGameplayTagContainer());

	FMassEntityHandle GetTargetEntity() const;
	FArcAttribute* ResolveAttribute(const FArcAttributeRef& AttrRef) const;
	bool ModifyInline(const FArcAttributeRef& AttrRef, float NewValue);

	/**
	 * Set the initial value of an attribute. Differs from ModifyInline:
	 * - Writes BOTH BaseValue and CurrentValue (via FArcAttribute::InitValue).
	 * - Fires Initialize unconditionally (no Old == New short-circuit) so the
	 *   handler cascade always seeds derived attributes — even when the initial
	 *   value is 0 or matches the default.
	 * - Skips PreChange (init is not a gated mutation).
	 *
	 * Returns false only when the property cannot be resolved on the fragment.
	 */
	bool InitInline(const FArcAttributeRef& AttrRef, float NewValue);
	float EvaluateWithTags(const FArcAttributeRef& AttrRef, float BaseValue,
	                       const FGameplayTagContainer& SourceTags,
	                       const FGameplayTagContainer& TargetTags) const;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAttributeHandler
{
	GENERATED_BODY()

	virtual ~FArcAttributeHandler() = default;

	virtual bool PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue) const { return true; }
	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const {}
	virtual void Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const {}
	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const {}

	virtual void OnObservedAttributeChanged(
		FArcAttributeHandlerContext& Context,
		FArcAttribute&               Attribute,
		const FArcAttributeRef&      ChangedRef,
		float                        OldValue,
		float                        NewValue) const {}

	virtual void GatherInputs(TArray<FArcAttributeRef>& OutInputs) const {}
};

UCLASS(BlueprintType)
class ARCMASSABILITIES_API UArcAttributeHandlerConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Handlers", meta=(BaseStruct="/Script/ArcMassAbilities.ArcAttributeHandler", ExcludeBaseStruct, ForceInlineRow))
	TMap<FArcAttributeRef, FInstancedStruct> Handlers;

	UPROPERTY(EditAnywhere, Category = "Aggregation", meta=(BaseStruct="/Script/ArcMassAbilities.ArcAggregationPolicy", ExcludeBaseStruct, ForceInlineRow))
	TMap<FArcAttributeRef, FInstancedStruct> AggregationPolicies;

	const FArcAttributeHandler* FindHandler(const FArcAttributeRef& AttributeRef) const;
	const FInstancedStruct* FindAggregationPolicy(const FArcAttributeRef& AttributeRef) const;

	/**
	 * Returns the list of observer attributes that watch @p Changed, or nullptr if none.
	 * Reads from the transient reverse index built by BuildReverseIndex().
	 */
	const TArray<FArcAttributeRef>* FindObserversFor(const FArcAttributeRef& Changed) const;

	/**
	 * Validates that the handler inputs (FArcAttributeHandler::GatherInputs) contain no cycles (including self-edges).
	 * Returns true if the graph is acyclic; false and sets OutError on cycle detection.
	 */
	bool ValidateNoCycles(FString& OutError) const;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Transient reverse index built from the handlers' GatherInputs: ObservedAttr -> [ObserverAttrs]. */
	TMap<FArcAttributeRef, TArray<FArcAttributeRef>> ObserversByObserved;

	void BuildReverseIndex();
};
