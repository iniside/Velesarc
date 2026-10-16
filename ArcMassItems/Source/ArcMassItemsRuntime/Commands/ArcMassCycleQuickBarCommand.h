// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "ArcMassCycleQuickBarCommand.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassCycleQuickBarCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Target Mass entity owning the quick bar group. */
	UPROPERTY()
	FMassEntityHandle Entity;

	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBarGroup;

	UPROPERTY(BlueprintReadWrite)
	int32 Direction = 1;

	UPROPERTY()
	FGameplayTag ResolvedQuickBar;

	TArray<FArcMassQuickBarBarSelection> PredictionCapture;

	FArcMassCycleQuickBarCommand() = default;

	FArcMassCycleQuickBarCommand(FMassEntityHandle InEntity,
		const FGameplayTag& InQuickBarGroup,
		int32 InDirection)
		: Entity(InEntity)
		, QuickBarGroup(InQuickBarGroup)
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
		return FArcMassCycleQuickBarCommand::StaticStruct();
	}
	virtual ~FArcMassCycleQuickBarCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassCycleQuickBarCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassCycleQuickBarCommand>
{
	enum
	{
		WithCopy = true
	};
};
