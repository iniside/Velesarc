#pragma once

#include "Decision/ArcTacticsNode.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "Considerations/ArcTacticsConsiderations.h"
#include "Curves/CurveFloat.h"
#include "Math/ArcResponseCurve.h"
#include "GameFramework/Actor.h"
#include "GlobalTasks/ArcTacticsDataTasks.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"
#include "StructUtils/InstancedStruct.h"
#include "UtilityAI/ArcUtilityConsideration.h"

#include "ArcTacticsTestTypes.generated.h"

USTRUCT(BlueprintType)
struct FArcTacticsTestRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	int32 Key = 0;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	float Score = 0.0f;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	FVector Location = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FArcTacticsTestNestedInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 Child = 0;
};

USTRUCT(BlueprintType)
struct FArcTacticsTestContextData : public FArcTacticsContextData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	int32 Scalar = 7;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	FArcTacticsTestRecord Record;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	TArray<FArcTacticsTestRecord> Records;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	int32 Input = 0;
};

USTRUCT(BlueprintType)
struct FArcTacticsTestTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	int32 Scalar = 0;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	FArcTacticsTestRecord Record;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	TArray<FArcTacticsTestRecord> Records;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	TArray<FMassEntityHandle> Entities;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	TArray<FVector> Locations;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	TArray<FArcTacticsTarget> Targets;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	FArcResponseCurve ResponseCurve;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	int32 Input = 0;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsParameter))
	uint8 Mode = 0;

	UPROPERTY(Transient)
	int32 TickCount = 0;

	UPROPERTY(Transient)
	int32 CancelCount = 0;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct FArcTacticsTestTask : public FArcTacticsGlobalTask
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsTestTaskInstanceData::StaticStruct();
	}

	virtual EArcTacticsTaskStatus Enter(FArcTacticsTaskContext&, FStructView InstanceData) const override
	{
		FArcTacticsTestTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsTestTaskInstanceData>();
		if (!Data)
		{
			return EArcTacticsTaskStatus::Failed;
		}
		Data->Scalar = 11;
		if (Data->Mode == 1)
		{
			return EArcTacticsTaskStatus::Running;
		}
		if (Data->Mode == 2)
		{
			Data->Scalar = 33;
			return EArcTacticsTaskStatus::Failed;
		}
		if (Data->Mode == 3)
		{
			Data->Scalar = Data->Input * 2;
		}
		if (Data->Mode == 4)
		{
			Data->ResponseCurve.Type = EArcResponseCurveType::Custom;
			FRichCurve* Curve = Data->ResponseCurve.CurveInfo.GetRichCurve();
			Curve->Reset();
			Curve->AddKey(0.0f, 1.0f);
			Curve->AddKey(1.0f, 0.0f);
		}
		if (Data->Mode == 5)
		{
			Data->ResponseCurve.Type = EArcResponseCurveType::Custom;
			FRichCurve* Curve = Data->ResponseCurve.CurveInfo.GetRichCurve();
			Curve->Reset();
			Curve->AddKey(0.0f, NAN);
			Curve->AddKey(1.0f, 1.0f);
		}
		if (Data->Mode == 8)
		{
			Data->Scalar = -1;
		}
		return EArcTacticsTaskStatus::Succeeded;
	}

	virtual EArcTacticsTaskStatus Tick(FArcTacticsTaskContext&, FStructView InstanceData) const override
	{
		FArcTacticsTestTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsTestTaskInstanceData>();
		if (!Data)
		{
			return EArcTacticsTaskStatus::Failed;
		}
		++Data->TickCount;
		Data->Scalar = 22;
		return EArcTacticsTaskStatus::Succeeded;
	}

	virtual void Cancel(FArcTacticsTaskContext&, FStructView InstanceData) const override
	{
		FArcTacticsTestTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsTestTaskInstanceData>();
		if (Data)
		{
			Data->Scalar = 44;
			++Data->CancelCount;
		}
	}
};

USTRUCT(BlueprintType)
struct FArcTacticsTestActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsRequiredBinding))
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsParameter))
	int32 Literal = 23;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	int32 Scalar = 0;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	FArcTacticsTestRecord Record;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	TArray<FArcTacticsTestRecord> Records;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	FArcTacticsTarget Target;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	float NestedValue = -13.0f;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	float OutputValue = 0.0f;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	int32 OutputInteger = 0;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	TArray<float> OutputValues;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	FArcTacticsTestNestedInput NestedInput;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct FArcTacticsTestAction : public FArcTacticsActionNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsTestActionInstanceData::StaticStruct();
	}
};

USTRUCT(BlueprintType)
struct FArcTacticsTestStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsRequiredBinding))
	float Required = 0.0f;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	float Optional = 0.0f;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	float Result = 0.0f;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct FArcTacticsTestStep : public FArcTacticsStepNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsTestStepInstanceData::StaticStruct();
	}
};

USTRUCT(BlueprintType)
struct FArcTacticsTestMissingRoleInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 ExposedWithoutRole = 0;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct FArcTacticsTestMissingRoleAction : public FArcTacticsActionNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsTestMissingRoleInstanceData::StaticStruct();
	}
};

USTRUCT(BlueprintType)
struct FArcTacticsTestConflictingRoleInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOutput))
	int32 ConflictingRole = 0;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct FArcTacticsTestConflictingRoleAction : public FArcTacticsActionNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsTestConflictingRoleInstanceData::StaticStruct();
	}
};

USTRUCT(BlueprintType, meta = (Hidden))
struct FArcTacticsTestConsiderationInstanceData : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsRequiredBinding))
	float BoundScore = 1.0f;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsParameter))
	bool bVeto = false;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsParameter))
	bool bPublishEight = false;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsInput, ArcTacticsOptional))
	float InputFromBag = 0.0f;

	UPROPERTY(EditAnywhere, meta = (ArcTacticsOutput))
	float PublishedScore = 0.0f;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget&, const FArcUtilityContext&) const override
	{
		return bVeto ? FArcUtilityConsiderationResult::Reject(TEXT("FixtureVeto"))
			: FArcUtilityConsiderationResult::Accept(BoundScore);
	}
};

USTRUCT(BlueprintType, meta = (Hidden))
struct FArcTacticsTestConsideration : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual FArcUtilityConsiderationResult Score(
		const FStructView InstanceData,
		const FArcUtilityTarget& Target,
		const FArcUtilityContext& Context) const override
	{
		FArcTacticsTestConsiderationInstanceData* Data = InstanceData.GetPtr<FArcTacticsTestConsiderationInstanceData>();
		if (!Data)
		{
			return FArcUtilityConsiderationResult::Reject(TEXT("InvalidFixtureConsiderationData"));
		}
		Data->PublishedScore = Data->bPublishEight
			? 0.8f
			: (FMath::IsNearlyZero(Data->InputFromBag) ? Data->BoundScore : -1.0f);
		return Data->Score(Target, Context);
	}

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsTestConsiderationInstanceData::StaticStruct();
	}
};

USTRUCT(BlueprintType, meta = (Hidden))
struct FArcTacticsTestBadConsideration : public FArcTacticsNode
{
	GENERATED_BODY()

	virtual EArcTacticsNodeRole GetRole() const override
	{
		return EArcTacticsNodeRole::Consideration;
	}

	virtual const UScriptStruct* GetInstanceDataType() const override
	{
		return FArcTacticsTestConsiderationInstanceData::StaticStruct();
	}
};
