// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "ArcMassAddBagCommand.generated.h"

class UArcBagDefinition;

/**
 * Server-side command that appends a new bag instance (sized to BagDefinition->SlotCount)
 * onto the target Mass entity's bag store fragment.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassAddBagCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Bag definition describing capacity and acceptance rules. */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcBagDefinition> BagDefinition = nullptr;

	FArcMassAddBagCommand() = default;

	FArcMassAddBagCommand(FMassEntityHandle InEntity, UArcBagDefinition* InBagDefinition)
		: FArcMassItemReplicatedCommand(InEntity)
		, BagDefinition(InBagDefinition)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassAddBagCommand::StaticStruct();
	}
	virtual ~FArcMassAddBagCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassAddBagCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassAddBagCommand>
{
	enum
	{
		WithCopy = true
	};
};
