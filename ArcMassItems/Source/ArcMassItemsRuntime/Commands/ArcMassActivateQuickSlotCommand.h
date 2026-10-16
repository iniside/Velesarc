// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "ArcMassActivateQuickSlotCommand.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassActivateQuickSlotCommand : public FArcReplicatedCommand
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

	FArcMassActivateQuickSlotCommand() = default;

	FArcMassActivateQuickSlotCommand(FMassEntityHandle InEntity,
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
		return FArcMassActivateQuickSlotCommand::StaticStruct();
	}
	virtual ~FArcMassActivateQuickSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassActivateQuickSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassActivateQuickSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
