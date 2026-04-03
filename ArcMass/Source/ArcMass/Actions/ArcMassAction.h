// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

#include "ArcMassAction.generated.h"

class UWorld;
struct FMassEntityManager;
struct FMassExecutionContext;

struct ARCMASS_API FArcMassActionContext
{
	FMassEntityManager& EntityManager;
	FMassExecutionContext& ExecutionContext;
	UWorld& World;
	FMassEntityHandle Entity;
};

USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassStatelessAction
{
	GENERATED_BODY()
	virtual ~FArcMassStatelessAction() = default;

	virtual void Execute(const FArcMassActionContext& Context) const {}
	virtual void PostExecute(const FArcMassActionContext& Context) const {}
};

USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassStatefulAction
{
	GENERATED_BODY()
	virtual ~FArcMassStatefulAction() = default;

	virtual void Execute(FArcMassActionContext& Context) {}
	virtual void PostExecute(FArcMassActionContext& Context) {}
};
