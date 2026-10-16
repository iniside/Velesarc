// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsBindings.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/InstancedStructContainer.h"
#include "StructUtils/PropertyBag.h"

#include "ArcTacticsOption.generated.h"

class FReferenceCollector;

UENUM()
enum class EArcTacticsCandidateOutcome : uint8
{
	Accepted,
	BelowMinScore,
	Vetoed,
	BindingFailed
};

UENUM()
enum class EArcTacticsValueIdentity : uint8
{
	None,
	Entity,
	Vector
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsCandidateIdentity
{
	GENERATED_BODY()

	UPROPERTY()
	EArcTacticsValueIdentity Type = EArcTacticsValueIdentity::None;

	UPROPERTY()
	FMassEntityHandle Entity;

	UPROPERTY()
	FVector Vector = FVector::ZeroVector;

	bool operator==(const FArcTacticsCandidateIdentity& Other) const;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsOption
{
	GENERATED_BODY()

	UPROPERTY()
	FArcTacticsNodeHandle Action;

	UPROPERTY()
	FName Name;

	UPROPERTY()
	int32 SourceIndex = INDEX_NONE;

	UPROPERTY()
	bool bFromSelector = false;

	UPROPERTY()
	FArcTacticsCandidateIdentity Identity;

	UPROPERTY()
	FInstancedStruct ActionData;

	UPROPERTY()
	FInstancedStructContainer StepData;

	UPROPERTY()
	FInstancedStructContainer ActiveStepData;

	UPROPERTY()
	FArcTacticsBranchParameters Parameters;

	UPROPERTY()
	FInstancedPropertyBag ParameterBag;

	UPROPERTY()
	FInstancedPropertyBag GlobalParameters;

	UPROPERTY()
	FInstancedStructContainer CapturedNodeData;

	UPROPERTY()
	int32 ItemIndex = INDEX_NONE;

	UPROPERTY()
	float Score = 0.0f;

	UPROPERTY()
	uint32 Serial = 0;

	UPROPERTY()
	int32 PackageIndex = INDEX_NONE;

	UPROPERTY()
	uint32 PackageSerial = 0;

	bool IsSet() const;
	bool HasSameIdentity(const FArcTacticsOption& Other) const;
	void Reset();
	void AddReferencedObjects(FReferenceCollector& Collector);
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsCandidate
{
	GENERATED_BODY()

	UPROPERTY()
	FArcTacticsOption Option;

	UPROPERTY()
	EArcTacticsCandidateOutcome Outcome = EArcTacticsCandidateOutcome::BelowMinScore;

	UPROPERTY()
	FName RejectionReason;

	UPROPERTY()
	int32 Order = INDEX_NONE;

	void AddReferencedObjects(FReferenceCollector& Collector);
};
