// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Knowledge/ArcKnowledgeEntryDefinitionFactory.h"
#include "ArcKnowledgeEntryDefinition.h"

UArcKnowledgeEntryDefinitionFactory::UArcKnowledgeEntryDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcKnowledgeEntryDefinition::StaticClass();
}

UObject* UArcKnowledgeEntryDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcKnowledgeEntryDefinition>(InParent, Class, Name, Flags);
}
