// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Knowledge/ArcKnowledgeQueryDefinitionFactory.h"
#include "ArcKnowledgeQueryDefinition.h"

UArcKnowledgeQueryDefinitionFactory::UArcKnowledgeQueryDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcKnowledgeQueryDefinition::StaticClass();
}

UObject* UArcKnowledgeQueryDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcKnowledgeQueryDefinition>(InParent, Class, Name, Flags);
}
