#pragma once

#include "Actions/ArcTacticsAction.h"
#include "Decision/ArcTacticsConfig.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"
#include "UObject/Object.h"

#include "ArcTacticsPackageTestTypes.generated.h"

UCLASS()
class UArcTacticsPackageProbe : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 EnterCount = 0;

	UPROPERTY()
	int32 TickCount = 0;

	UPROPERTY()
	int32 CancelCount = 0;

	UPROPERTY()
	int32 CallbackCount = 0;

	UPROPERTY()
	bool bFailEnter = false;

	UPROPERTY()
	EArcTacticsActionCancelMode LastCancelMode = EArcTacticsActionCancelMode::PreserveHold;

	UPROPERTY()
	TArray<TObjectPtr<const UArcTacticsConfig>> TickConfigs;

	UPROPERTY()
	TArray<bool> TickAdds;

	UPROPERTY()
	TArray<TObjectPtr<const UArcTacticsConfig>> CancelConfigs;

	UPROPERTY()
	TArray<bool> CancelAdds;

	UPROPERTY()
	TArray<TObjectPtr<const UArcTacticsConfig>> CallbackConfigs;

	UPROPERTY()
	TArray<bool> CallbackAdds;

	UPROPERTY()
	TArray<bool> TickResults;

	UPROPERTY()
	TArray<bool> CancelResults;

	UPROPERTY()
	TArray<bool> CallbackResults;

	UPROPERTY()
	int32 PackagesDuringTick = INDEX_NONE;

	UPROPERTY()
	int32 PendingDuringTick = INDEX_NONE;

	UPROPERTY()
	int32 PackagesDuringCancel = INDEX_NONE;

	UPROPERTY()
	int32 PackagesDuringCallback = INDEX_NONE;
};

USTRUCT()
struct FArcTacticsPackageTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsParameter))
	uint8 Mode = 0;

	UPROPERTY()
	TObjectPtr<UArcTacticsPackageProbe> Probe = nullptr;
};

USTRUCT(meta = (Hidden))
struct FArcTacticsPackageTask : public FArcTacticsGlobalTask
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsPackageTaskInstanceData::StaticStruct();
	}

	virtual EArcTacticsTaskStatus Enter(FArcTacticsTaskContext&, FStructView InstanceData) const override;
	virtual EArcTacticsTaskStatus Tick(FArcTacticsTaskContext&, FStructView InstanceData) const override;
	virtual void Cancel(FArcTacticsTaskContext&, FStructView InstanceData) const override;
	virtual void OnAsyncQueryCompleted(
		FArcTacticsTaskContext&,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken&,
		FArcTQSQueryInstance&) const override;
};

USTRUCT(meta = (Hidden))
struct FArcTacticsPackageTeamTask : public FArcTacticsPackageTask
{
	GENERATED_BODY()

	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView) const override
	{
		return EArcTacticsExecutionRequirement::Team;
	}
};

USTRUCT()
struct FArcTacticsPackageScriptActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsParameter))
	bool bAcquireHold = false;

	UPROPERTY()
	TObjectPtr<UArcTacticsPackageProbe> Probe = nullptr;
};

USTRUCT(meta = (Hidden))
struct FArcTacticsPackageScriptAction : public FArcTacticsActionNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsPackageScriptActionInstanceData::StaticStruct();
	}

	virtual EArcTacticsActionStatus Enter(FArcTacticsActionContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsActionContext& Context, FStructView InstanceData) const override;
	virtual void Cancel(
		FArcTacticsActionContext& Context,
		FStructView InstanceData,
		EArcTacticsActionCancelMode CancelMode) const override;
	virtual EArcTacticsActionTickLane GetTickLane(const FArcTacticsActionView&) const override
	{
		return EArcTacticsActionTickLane::Utility;
	}
};

USTRUCT()
struct FArcTacticsPackageStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	int32 TickCount = 0;
};

USTRUCT(meta = (Hidden))
struct FArcTacticsPackageStep : public FArcTacticsStepNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsPackageStepInstanceData::StaticStruct();
	}

	virtual bool Enter(FArcTacticsStepContext&, FStructView) const override
	{
		return true;
	}

	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext&, FStructView InstanceData) const override;

	virtual EArcTacticsActionTickLane GetTickLane(FConstStructView) const override
	{
		return EArcTacticsActionTickLane::Utility;
	}
};
