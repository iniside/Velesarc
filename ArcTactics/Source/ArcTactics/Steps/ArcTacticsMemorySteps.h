// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsAction.h"
#include "CoreMinimal.h"
#include "Decision/ArcTacticsNode.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

#include "ArcTacticsMemorySteps.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Runtime state shared by the memory steps."))
struct ARCTACTICS_API FArcTacticsMemoryStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	double WakeTime = 0.0;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Key and value of a Write Memory Location step."))
struct ARCTACTICS_API FArcTacticsWriteMemoryLocationStepInstanceData : public FArcTacticsMemoryStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, Categories = "AI.Memory", ArcMemoryAccess = "Write", ArcMemoryType = "/Script/CoreUObject.Vector", ToolTip = "NPC memory key the step writes."))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "World location in cm the step remembers under Key."))
	FVector Value = FVector::ZeroVector;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Key and value of a Write Memory Entity step."))
struct ARCTACTICS_API FArcTacticsWriteMemoryEntityStepInstanceData : public FArcTacticsMemoryStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, Categories = "AI.Memory", ArcMemoryAccess = "Write", ArcMemoryType = "/Script/MassCore.MassEntityHandle", ToolTip = "NPC memory key the step writes."))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "Entity the step remembers under Key."))
	FMassEntityHandle Value;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Key and value of a Write Memory Float step."))
struct ARCTACTICS_API FArcTacticsWriteMemoryFloatStepInstanceData : public FArcTacticsMemoryStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, Categories = "AI.Memory", ArcMemoryAccess = "Write", ArcMemoryType = "/Script/ArcAI.ArcMemoryFloat", ToolTip = "NPC memory key the step writes."))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "Float the step remembers under Key."))
	float Value = 0.0f;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Key and value of a Write Memory Bool step."))
struct ARCTACTICS_API FArcTacticsWriteMemoryBoolStepInstanceData : public FArcTacticsMemoryStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, Categories = "AI.Memory", ArcMemoryAccess = "Write", ArcMemoryType = "/Script/ArcAI.ArcMemoryBool", ToolTip = "NPC memory key the step writes."))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "Bool the step remembers under Key."))
	bool bValue = false;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Key and value of a Write Memory Tag step."))
struct ARCTACTICS_API FArcTacticsWriteMemoryTagStepInstanceData : public FArcTacticsMemoryStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, Categories = "AI.Memory", ArcMemoryAccess = "Write", ArcMemoryType = "/Script/GameplayTags.GameplayTag", ToolTip = "NPC memory key the step writes."))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "Gameplay tag the step remembers under Key."))
	FGameplayTag Value;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Key of a Forget Memory step."))
struct ARCTACTICS_API FArcTacticsForgetMemoryStepInstanceData : public FArcTacticsMemoryStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, Categories = "AI.Memory", ArcMemoryAccess = "Forget", ToolTip = "NPC memory key the step forgets, whatever type it holds."))
	FGameplayTag Key;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCTACTICS_API FArcTacticsMemoryStep : public FArcTacticsStepNode
{
	GENERATED_BODY()

	virtual bool Enter(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsActionTickLane GetTickLane(FConstStructView InstanceData) const override;
	virtual double GetNextWakeTime(FConstStructView InstanceData) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Write Memory Location", ToolTip = "Remembers Value under Key in the entity's NPC memory; fails when the entity has no memory."))
struct ARCTACTICS_API FArcTacticsWriteMemoryLocationStep : public FArcTacticsMemoryStep
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Write Memory Entity", ToolTip = "Remembers Value under Key in the entity's NPC memory; fails when the entity has no memory."))
struct ARCTACTICS_API FArcTacticsWriteMemoryEntityStep : public FArcTacticsMemoryStep
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Write Memory Float", ToolTip = "Remembers Value under Key in the entity's NPC memory; fails when the entity has no memory."))
struct ARCTACTICS_API FArcTacticsWriteMemoryFloatStep : public FArcTacticsMemoryStep
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Write Memory Bool", ToolTip = "Remembers Value under Key in the entity's NPC memory; fails when the entity has no memory."))
struct ARCTACTICS_API FArcTacticsWriteMemoryBoolStep : public FArcTacticsMemoryStep
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Write Memory Tag", ToolTip = "Remembers Value under Key in the entity's NPC memory; fails when the entity has no memory."))
struct ARCTACTICS_API FArcTacticsWriteMemoryTagStep : public FArcTacticsMemoryStep
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Forget Memory", ToolTip = "Removes Key from the entity's NPC memory; fails when the entity has no memory."))
struct ARCTACTICS_API FArcTacticsForgetMemoryStep : public FArcTacticsMemoryStep
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
};
