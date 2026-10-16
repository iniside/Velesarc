// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "ArcMassDeactivateQuickSlotCommand.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassDeactivateQuickSlotCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Target Mass entity owning the quick bar. */
	UPROPERTY()
	FMassEntityHandle Entity;

	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickSlot;

	TArray<FArcMassQuickBarBarSelection> PredictionCapture;

	FArcMassDeactivateQuickSlotCommand() = default;

	FArcMassDeactivateQuickSlotCommand(FMassEntityHandle InEntity,
		const FGameplayTag& InQuickBar,
		const FGameplayTag& InQuickSlot)
		: Entity(InEntity)
		, QuickBar(InQuickBar)
		, QuickSlot(InQuickSlot)
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
		return FArcMassDeactivateQuickSlotCommand::StaticStruct();
	}
	virtual ~FArcMassDeactivateQuickSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassDeactivateQuickSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassDeactivateQuickSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
