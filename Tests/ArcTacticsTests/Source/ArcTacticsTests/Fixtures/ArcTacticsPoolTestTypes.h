#pragma once

#include "Actions/ArcTacticsAction.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"
#include "Mass/Processor.h"
#include "UObject/Object.h"

#include "ArcTacticsPoolTestTypes.generated.h"

UCLASS()
class UArcTacticsPoolTestObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 CancelCount = 0;
};

UCLASS()
class UArcTacticsPoolOutputSink : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Value = 0;
};

USTRUCT()
struct FArcTacticsPoolCallbackTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 CallbackCount = 0;

	UPROPERTY()
	TObjectPtr<UArcTacticsPoolTestObject> StrongReference = nullptr;

	UPROPERTY()
	TObjectPtr<UArcTacticsPoolTestObject> CancelTarget = nullptr;
};

USTRUCT(meta = (Hidden))
struct FArcTacticsPoolCallbackTask : public FArcTacticsGlobalTask
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsPoolCallbackTaskInstanceData::StaticStruct();
	}

	virtual void OnAsyncQueryCompleted(
		FArcTacticsTaskContext&,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken&,
		FArcTQSQueryInstance&) const override;

	virtual void Cancel(FArcTacticsTaskContext&, FStructView InstanceData) const override;
};

USTRUCT()
struct FArcTacticsPoolCallbackActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 CallbackCount = 0;

	UPROPERTY()
	TObjectPtr<UArcTacticsPoolTestObject> StrongReference = nullptr;

	UPROPERTY()
	TObjectPtr<UArcTacticsPoolTestObject> CancelTarget = nullptr;
};

USTRUCT(meta = (Hidden))
struct FArcTacticsPoolCallbackAction : public FArcTacticsActionNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsPoolCallbackActionInstanceData::StaticStruct();
	}

	virtual void OnAsyncQueryCompleted(
		FArcTacticsActionContext&,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken&,
		FArcTQSQueryInstance&) const override;

	virtual void Cancel(
		FArcTacticsActionContext&,
		FStructView InstanceData,
		EArcTacticsActionCancelMode) const override;
};

USTRUCT()
struct FArcTacticsPoolLifecycleActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	int32 Output = 0;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput))
	int32 CapturedGlobal = 0;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsParameter))
	uint8 Mode = 0;

	UPROPERTY(Transient)
	int32 EnterCount = 0;

	UPROPERTY(Transient)
	int32 TickCount = 0;

	UPROPERTY()
	TObjectPtr<UArcTacticsPoolTestObject> CancelTarget = nullptr;
};

USTRUCT(meta = (Hidden))
struct FArcTacticsPoolLifecycleAction : public FArcTacticsActionNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsPoolLifecycleActionInstanceData::StaticStruct();
	}

	virtual EArcTacticsActionStatus Enter(FArcTacticsActionContext&, FStructView InstanceData) const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsActionContext&, FStructView InstanceData) const override;
	virtual void Cancel(
		FArcTacticsActionContext&,
		FStructView InstanceData,
		EArcTacticsActionCancelMode) const override;
	virtual EArcTacticsActionTickLane GetTickLane(const FArcTacticsActionView&) const override
	{
		return EArcTacticsActionTickLane::Utility;
	}
};

UCLASS()
class UArcTacticsPoolDrainProcessor : public UMassProcessor
{
	GENERATED_BODY()

protected:
	virtual void Execute(FMassEntityManager&, FMassExecutionContext&) override;
};
