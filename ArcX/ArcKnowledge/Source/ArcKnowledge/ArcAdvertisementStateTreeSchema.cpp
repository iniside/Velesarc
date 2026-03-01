// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAdvertisementStateTreeSchema.h"
#include "ArcKnowledgeTypes.h"
#include "MassEntityHandle.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConsiderationBase.h"
#include "Conditions/StateTreeAIConditionBase.h"
#include "Tasks/StateTreeAITask.h"
#include "Subsystems/WorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAdvertisementStateTreeSchema)

namespace UE::ArcKnowledge::Names
{
	const FName ContextActor = TEXT("ContextActor");
	const FName ExecutingEntityHandle = TEXT("ExecutingEntity");
	const FName SourceEntityHandle = TEXT("SourceEntity");
	const FName AdvertisementHandle = TEXT("AdvertisementHandle");
}

UArcAdvertisementStateTreeSchema::UArcAdvertisementStateTreeSchema()
	: ContextActorClass(AActor::StaticClass())
	, ContextDataDescs({
		{ UE::ArcKnowledge::Names::ContextActor, AActor::StaticClass(), FGuid(0xA1B2C3D4, 0xE5F6A7B8, 0xC9D0E1F2, 0xA3B4C5D6) },
		{ UE::ArcKnowledge::Names::ExecutingEntityHandle, FMassEntityHandle::StaticStruct(), FGuid(0xD7E8F9A0, 0xB1C2D3E4, 0xF5A6B7C8, 0xD9E0F1A2) },
		{ UE::ArcKnowledge::Names::SourceEntityHandle, FMassEntityHandle::StaticStruct(), FGuid(0xB3C4D5E6, 0xF7A8B9C0, 0xD1E2F3A4, 0xB5C6D7E8) },
		{ UE::ArcKnowledge::Names::AdvertisementHandle, FArcKnowledgeHandle::StaticStruct(), FGuid(0xF9A0B1C2, 0xD3E4F5A6, 0xB7C8D9E0, 0xF1A2B3C4) },
	})
{
	// ContextActor is optional — advertisements can execute without an associated actor (pure Mass entities)
	ContextDataDescs[0].Requirement = EStateTreeExternalDataRequirement::Optional;
}

bool UArcAdvertisementStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeAITaskBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeAIConditionBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeConsiderationCommonBase::StaticStruct());
}

bool UArcAdvertisementStateTreeSchema::IsClassAllowed(const UClass* InClass) const
{
	return IsChildOfBlueprintBase(InClass);
}

bool UArcAdvertisementStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(AActor::StaticClass())
		|| InStruct.IsChildOf(UActorComponent::StaticClass())
		|| InStruct.IsChildOf(UWorldSubsystem::StaticClass());
}

void UArcAdvertisementStateTreeSchema::PostLoad()
{
	Super::PostLoad();
	ContextDataDescs[0].Struct = ContextActorClass.Get();
}

#if WITH_EDITOR
void UArcAdvertisementStateTreeSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FProperty* Property = PropertyChangedEvent.Property;
	if (Property
		&& Property->GetOwnerClass() == UArcAdvertisementStateTreeSchema::StaticClass()
		&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UArcAdvertisementStateTreeSchema, ContextActorClass))
	{
		ContextDataDescs[0].Struct = ContextActorClass.Get();
	}
}
#endif
