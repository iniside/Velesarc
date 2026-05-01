// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "MassSmartObjectRequest.h"
#include "GameplayTagContainer.h"
#include "ArcTransportReadAssignmentTask.generated.h"

class UArcItemDefinition;

USTRUCT()
struct FArcTransportReadAssignmentTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassEntityHandle SourceBuildingHandle;

	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassEntityHandle DestinationBuildingHandle;

	UPROPERTY(VisibleAnywhere, Category = Output)
	TObjectPtr<UArcItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Output)
	int32 Quantity = 0;

	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bHasAssignment = false;

	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassSmartObjectCandidateSlots SourceCandidateSlots;

	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassSmartObjectCandidateSlots DestinationCandidateSlots;

	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bHasSourceSlots = false;

	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bHasDestinationSlots = false;
};

USTRUCT(meta = (DisplayName = "Arc Transport Read Assignment", Category = "Economy"))
struct ARCECONOMY_API FArcTransportReadAssignmentTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcTransportReadAssignmentTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(100, 180, 60); }
#endif

protected:
	TStateTreeExternalDataHandle<FArcTransporterFragment> TransporterFragmentHandle;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagQuery SourceSlotRequirements;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagQuery DestinationSlotRequirements;
};
