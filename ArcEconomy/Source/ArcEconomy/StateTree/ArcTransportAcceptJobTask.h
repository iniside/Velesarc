// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcTransportAcceptJobTask.generated.h"

class UArcItemDefinition;

USTRUCT()
struct FArcTransportAcceptJobTaskInstanceData
{
	GENERATED_BODY()

	/** Input: handle to the claimed transport advertisement. */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcKnowledgeHandle AdvertisementHandle;

	/** Output: building to pick up from (supply source). */
	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassEntityHandle SupplyBuildingHandle;

	/** Output: building to deliver to (demand target, from advertisement). */
	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassEntityHandle DemandBuildingHandle;

	/** Output: item to transport (concrete — resolved from supply for tag-based demands). */
	UPROPERTY(VisibleAnywhere, Category = Output)
	TObjectPtr<UArcItemDefinition> ItemDefinition = nullptr;

	/** Output: original demand tags (non-empty for tag-based ingredient demands). */
	UPROPERTY(VisibleAnywhere, Category = Output)
	FGameplayTagContainer RequiredTags;

	/** Output: quantity to transport. */
	UPROPERTY(VisibleAnywhere, Category = Output)
	int32 Quantity = 0;
};

/**
 * Extracts job data from a claimed transport advertisement and finds
 * the best matching supply source. Populates FArcTransporterFragment
 * for downstream pick up / deliver tasks.
 *
 * Returns Failed if the advertisement is invalid or no supply match found.
 */
USTRUCT(meta = (DisplayName = "Arc Transport Accept Job", Category = "Economy"))
struct ARCECONOMY_API FArcTransportAcceptJobTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcTransportAcceptJobTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(100, 180, 60); }
#endif

protected:
	TStateTreeExternalDataHandle<FArcEconomyNPCFragment> NPCFragmentHandle;
	TStateTreeExternalDataHandle<FArcTransporterFragment> TransporterFragmentHandle;
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};
