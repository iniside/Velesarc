// Copyright Lukasz Baran. All Rights Reserved.

#include "Cues/ArcCueStateTreeSchema.h"

#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTaskBase.h"
#include "Subsystems/WorldSubsystem.h"

#include "Cues/ArcCueStateTreeContext.h"
#include "Cues/ArcCueStateTreeTaskBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueStateTreeSchema)

UArcCueStateTreeSchema::UArcCueStateTreeSchema()
	: ContextDataDescs({
		{ ArcAbilityKit::CueStateTree::ContextName, FArcCueStateTreeContext::StaticStruct(),
			FGuid(0x753C2B39, 0x82FD4AAF, 0xBC0CF652, 0x9E161F44) },
	})
{
}

bool UArcCueStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct != nullptr
		&& (InScriptStruct->IsChildOf(FArcCueStateTreeTaskBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct()));
}

bool UArcCueStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(UWorldSubsystem::StaticClass());
}
