// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Items/ArcItemSpec.h"
#include "ArcMassAddItemCommand.generated.h"

/**
 * Server-side command that adds a new item (described by an FArcItemSpec) to the
 * target Mass entity's item store fragment.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassAddItemCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Spec describing the item to instantiate. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemSpec Spec;

	FArcMassAddItemCommand() = default;

	FArcMassAddItemCommand(FMassEntityHandle InEntity,
		const FArcItemSpec& InSpec,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, Spec(InSpec)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassAddItemCommand::StaticStruct();
	}
	virtual ~FArcMassAddItemCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassAddItemCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassAddItemCommand>
{
	enum
	{
		WithCopy = true
	};
};
