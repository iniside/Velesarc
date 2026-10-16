// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/ArcItemStackMethod.h"
#include "Mass/EntityHandle.h"
#include "ArcMassItemStackMethods.generated.h"

struct FMassEntityManager;

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemStackMethod_StackByType : public FArcItemStackMethod_StackByType
{
	GENERATED_BODY()

public:
	virtual bool CanStackMass(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& InSpec) const override;
	virtual FArcItemId StackCheckMass(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const override;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemStackMethod_CanNotStackUnique : public FArcItemStackMethod_CanNotStackUnique
{
	GENERATED_BODY()

public:
	virtual bool CanAddMass(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& InSpec) const override;
};
