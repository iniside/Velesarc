// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "ArcMassCycleQuickSlotCommand.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassCycleQuickSlotCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Target Mass entity owning the quick bar. */
	UPROPERTY()
	FMassEntityHandle Entity;

	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickSlot;

	UPROPERTY(BlueprintReadWrite)
	int32 Direction = 1;

	UPROPERTY()
	FGameplayTag ResolvedQuickSlot;

	TArray<FArcMassQuickBarBarSelection> PredictionCapture;

	FArcMassCycleQuickSlotCommand() = default;

	FArcMassCycleQuickSlotCommand(FMassEntityHandle InEntity,
		const FGameplayTag& InQuickBar,
		const FGameplayTag& InQuickSlot,
		int32 InDirection)
		: Entity(InEntity)
		, QuickBar(InQuickBar)
		, QuickSlot(InQuickSlot)
		, Direction(InDirection)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual bool ExecutesOnClient() const override
	{
		return true;
	}
	virtual void UndoPrediction(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassCycleQuickSlotCommand::StaticStruct();
	}
	virtual ~FArcMassCycleQuickSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassCycleQuickSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassCycleQuickSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
