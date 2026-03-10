/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcCore/Conditions/ArcWorldConditionSchema.h"
#include "ArcCore/Conditions/ArcWorldConditionBase.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTypes.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcWorldConditionSchema)

UArcWorldConditionSchema::UArcWorldConditionSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SourceEntityRef = AddContextDataDesc(TEXT("SourceEntity"), FMassEntityHandle::StaticStruct(), EWorldConditionContextDataType::Dynamic);
	SourceActorRef = AddContextDataDesc(TEXT("SourceActor"), AActor::StaticClass(), EWorldConditionContextDataType::Dynamic);
	MassEntitySubsystemRef = AddContextDataDesc(TEXT("MassEntitySubsystem"), UMassEntitySubsystem::StaticClass(), EWorldConditionContextDataType::Persistent);
}

bool UArcWorldConditionSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	check(InScriptStruct);
	return Super::IsStructAllowed(InScriptStruct)
		|| InScriptStruct->IsChildOf(TBaseStructure<FWorldConditionCommonBase>::Get())
		|| InScriptStruct->IsChildOf(TBaseStructure<FWorldConditionCommonActorBase>::Get())
		|| InScriptStruct->IsChildOf(TBaseStructure<FArcWorldConditionBase>::Get());
}
