// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Mass/EntityHandle.h"
#include "ArcMassItemReplicatedCommand.generated.h"

class UScriptStruct;
class UWorld;

ARCMASSITEMSRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogArcReplicatedCommand, Log, All);

/**
 * Intermediate base for replicated commands that target a Mass entity's item store fragment.
 * Concrete Mass commands derive from this.
 *
 * The world used to resolve UMassEntitySubsystem on the server is passed as a
 * WorldContext argument to Execute() by the command dispatcher — Mass commands
 * no longer carry a sender-stamped world member.
 */
USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemReplicatedCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

public:
	/** Target Mass entity owning the item store fragment. */
	UPROPERTY()
	FMassEntityHandle Entity;

	/** Type of the item store fragment to operate on. Defaults to FArcMassItemStoreFragment::StaticStruct(). */
	UPROPERTY()
	TObjectPtr<UScriptStruct> StoreType = nullptr;

	FArcMassItemReplicatedCommand() = default;

	explicit FArcMassItemReplicatedCommand(FMassEntityHandle InEntity,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: Entity(InEntity)
		, StoreType(InStoreType)
	{}

	virtual ~FArcMassItemReplicatedCommand() override = default;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassItemReplicatedCommand::StaticStruct();
	}

protected:
	/** Resolve a UWorld from a generic WorldContext object passed by the command dispatcher. */
	UWorld* ResolveWorld(UObject* WorldContext) const
	{
		return WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	}
};

template <>
struct TStructOpsTypeTraits<FArcMassItemReplicatedCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassItemReplicatedCommand>
{
	enum
	{
		WithCopy = true
	};
};
