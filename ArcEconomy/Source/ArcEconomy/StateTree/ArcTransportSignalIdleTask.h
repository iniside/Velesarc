// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcTransportSignalIdleTask.generated.h"

USTRUCT()
struct FArcTransportSignalIdleTaskInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "Arc Transport Signal Idle", Category = "Economy"))
struct ARCECONOMY_API FArcTransportSignalIdleTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcTransportSignalIdleTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(100, 180, 60); }
#endif

protected:
	TStateTreeExternalDataHandle<FArcTransporterFragment> TransporterFragmentHandle;
	TStateTreeExternalDataHandle<FArcEconomyNPCFragment> NPCFragmentHandle;
};
