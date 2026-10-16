// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "ArcMassActivateQuickBarCommand.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassActivateQuickBarCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Target Mass entity owning the quick bar. */
	UPROPERTY()
	FMassEntityHandle Entity;

	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	TArray<FArcMassQuickBarBarSelection> PredictionCapture;

	FArcMassActivateQuickBarCommand() = default;

	FArcMassActivateQuickBarCommand(FMassEntityHandle InEntity,
		const FGameplayTag& InQuickBar)
		: Entity(InEntity)
		, QuickBar(InQuickBar)
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
		return FArcMassActivateQuickBarCommand::StaticStruct();
	}
	virtual ~FArcMassActivateQuickBarCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassActivateQuickBarCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassActivateQuickBarCommand>
{
	enum
	{
		WithCopy = true
	};
};
