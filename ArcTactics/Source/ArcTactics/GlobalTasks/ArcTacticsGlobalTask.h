// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsNode.h"
#include "Mass/ArcTacticsInstanceHandle.h"
#include "Mass/EntityHandle.h"

#include "ArcTacticsGlobalTask.generated.h"

class UWorld;
class UArcTacticsInstanceSubsystem;
class UArcTQSQueryDefinition;
struct FArcTQSQueryInstance;
struct FMassEntityManager;

struct ARCTACTICS_API FArcTacticsAsyncRequestToken
{
	TWeakObjectPtr<UArcTacticsInstanceSubsystem> InstanceSubsystem;
	FMassEntityHandle Entity;
	FArcTacticsInstanceHandle InstanceHandle;
	uint32 DecisionGeneration = 0;
	uint32 ActionSerial = 0;
	uint32 RequestGeneration = 0;
};

DECLARE_DELEGATE_TwoParams(
	FArcTacticsTaskAsyncCompletion,
	const FArcTacticsAsyncRequestToken&,
	FArcTQSQueryInstance&);

UENUM()
enum class EArcTacticsTaskStatus : uint8
{
	Succeeded,
	Running,
	Failed
};

struct FArcTacticsQueryReport
{
	const UArcTQSQueryDefinition* Definition = nullptr;
	FName SourceName;
	int32 AppliedCount = 0;
	bool bAccepted = false;
};

struct ARCTACTICS_API FArcTacticsTaskContext
{
	UWorld* World = nullptr;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	TWeakObjectPtr<UArcTacticsInstanceSubsystem> InstanceSubsystem;
	FArcTacticsInstanceHandle InstanceHandle;
	FArcTacticsTaskAsyncCompletion AsyncCompletion;
	uint32 DecisionGeneration = 0;
	float DeltaTime = 0.0f;
	FName FailureReason;
	FArcTacticsQueryReport QueryReport;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCTACTICS_API FArcTacticsGlobalTask : public FArcTacticsNode
{
	GENERATED_BODY()

	virtual EArcTacticsNodeRole GetRole() const override;
	virtual EArcTacticsTaskStatus Enter(FArcTacticsTaskContext& Context, FStructView InstanceData) const;
	virtual EArcTacticsTaskStatus Tick(FArcTacticsTaskContext& Context, FStructView InstanceData) const;
	virtual void Cancel(FArcTacticsTaskContext& Context, FStructView InstanceData) const;
	virtual void OnAsyncQueryCompleted(
		FArcTacticsTaskContext& Context,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken& Token,
		FArcTQSQueryInstance& CompletedQuery) const;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const;
};
