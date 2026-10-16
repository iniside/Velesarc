// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsMovement.h"
#include "ArcTQSPropertyFunction.h"
#include "CoreMinimal.h"
#include "Decision/ArcTacticsBindings.h"
#include "Decision/ArcTacticsNode.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcTacticsAction.generated.h"

class UArcAbilityStateTreeSubsystem;
class UArcTQSQuerySubsystem;
struct FArcTQSQueryInstance;
struct FArcTacticsCompiledData;
struct FArcTacticsInstanceData;
struct FArcTacticsOption;
struct FMassEntityManager;

namespace ArcPathFollow
{
	struct FContext;
}

UENUM(BlueprintType)
enum class EArcTacticsAbilityMovementPolicy : uint8
{
	HoldPosition,
	PreserveMovement
};

UENUM()
enum class EArcTacticsActionTickLane : uint8
{
	None,
	Utility,
	Movement
};

UENUM()
enum class EArcTacticsActionCancelMode : uint8
{
	PreserveHold,
	ReleaseHold
};

DECLARE_DELEGATE_TwoParams(
	FArcTacticsActionAsyncCompletion,
	const FArcTacticsAsyncRequestToken&,
	FArcTQSQueryInstance&);

struct ARCTACTICS_API FArcTacticsStepAccess
{
	FArcTacticsStepAccess() = default;
	FArcTacticsStepAccess(
		const FArcTacticsCompiledData& InCompiledData,
		FArcTacticsOption& InOption,
		FArcTacticsInstanceData* InBindingInstance,
		const FArcTQSFunctionContext& InFunctionContext);

	int32 Num() const;
	FArcTacticsNodeHandle GetHandle(int32 StepIndex) const;
	const FArcTacticsStepNode* GetNode(int32 StepIndex) const;
	FName GetName(int32 StepIndex) const;
	FStructView GetData(int32 StepIndex) const;
	bool ApplyStepInputs(int32 StepIndex) const;
	bool PublishStepOutputs(int32 StepIndex) const;

private:
	bool ApplyStepBindings(int32 StepIndex, bool bOutputs) const;

	const FArcTacticsCompiledData* CompiledData = nullptr;
	FArcTacticsOption* Option = nullptr;
	FArcTacticsInstanceData* BindingInstance = nullptr;
	FArcTQSFunctionContext FunctionContext;
	TConstArrayView<FArcTacticsNodeHandle> Handles;
};

struct ARCTACTICS_API FArcTacticsActiveStepAccess
{
	FArcTacticsActiveStepAccess() = default;
	FArcTacticsActiveStepAccess(
		const FArcTacticsCompiledData& InCompiledData,
		FArcTacticsOption& InOption,
		FArcTacticsInstanceData* InBindingInstance,
		const FArcTQSFunctionContext& InFunctionContext);

	int32 Num() const;
	FArcTacticsNodeHandle GetHandle(int32 ActiveStepIndex) const;
	const FArcTacticsActiveStepNode* GetNode(int32 ActiveStepIndex) const;
	FName GetName(int32 ActiveStepIndex) const;
	FStructView GetData(int32 ActiveStepIndex) const;
	bool ApplyInputs(int32 ActiveStepIndex) const;

private:
	const FArcTacticsCompiledData* CompiledData = nullptr;
	FArcTacticsOption* Option = nullptr;
	FArcTacticsInstanceData* BindingInstance = nullptr;
	FArcTQSFunctionContext FunctionContext;
	TConstArrayView<FArcTacticsNodeHandle> Handles;
};

struct ARCTACTICS_API FArcTacticsActionView
{
	FArcTacticsActionView() = default;
	FArcTacticsActionView(const FArcTacticsCompiledData& CompiledData, FArcTacticsOption& Option);

	FStructView ActionData;
	FArcTacticsStepAccess Steps;
	FArcTacticsActiveStepAccess ActiveSteps;
};

struct ARCTACTICS_API FArcTacticsActionContext
{
	UWorld* World = nullptr;
	FMassEntityManager* EntityManager = nullptr;
	UArcTQSQuerySubsystem* QuerySubsystem = nullptr;
	UArcAbilityStateTreeSubsystem* AbilitySubsystem = nullptr;
	const ArcPathFollow::FContext* PathContext = nullptr;
	FArcTacticsMovementHoldState* HoldState = nullptr;
	FMassEntityHandle Entity;
	FArcTacticsAsyncRequestToken RequestToken;
	FArcTacticsActionAsyncCompletion AsyncCompletion;
	FSimpleDelegate RequestExecution;
	FArcTacticsStepAccess Steps;
	FArcTacticsActiveStepAccess ActiveSteps;
	FArcTacticsQueryReport QueryReport;
	FName FailureReason;
};

USTRUCT(BlueprintType, meta = (ToolTip = "How an ability step treats movement and how long it waits for the ability to start and finish."))
struct ARCTACTICS_API FArcTacticsAbilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ToolTip = "Hold Position holds the entity still while the ability starts and runs; Preserve Movement releases any held position."))
	EArcTacticsAbilityMovementPolicy MovementPolicy = EArcTacticsAbilityMovementPolicy::HoldPosition;

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Seconds to wait for the ability to become active before the step fails."))
	float StartTimeout = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Seconds the active ability may run before it is cancelled and the step fails."))
	float ExecutionTimeout = 10.0f;
};

struct ARCTACTICS_API FArcTacticsStepContext
{
	FArcTacticsStepContext(
		FArcTacticsActionContext& InAction,
		uint32& InRequestGeneration,
		uint32& InRequestDecisionGeneration);

	FArcTacticsAsyncRequestToken BeginRequest();
	void InvalidateRequests();
	bool IsCurrentRequest(const FArcTacticsAsyncRequestToken& Token) const;

	FArcTacticsActionContext& Action;
	uint32& RequestGeneration;
	uint32& RequestDecisionGeneration;
};
