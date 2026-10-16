// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryDefinitionActorFactory.h"

#include "ArcTQSQueryDefinition.h"
#include "ArcTQSTestingActor.h"
#include "AssetRegistry/AssetData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSQueryDefinitionActorFactory)

#define LOCTEXT_NAMESPACE "ArcTQSQueryDefinitionActorFactory"

UArcTQSQueryDefinitionActorFactory::UArcTQSQueryDefinitionActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("DisplayName", "TQS Testing Actor");
	NewActorClass = AArcTQSTestingActor::StaticClass();
}

bool UArcTQSQueryDefinitionActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.IsInstanceOf(UArcTQSQueryDefinition::StaticClass()))
	{
		OutErrorMsg = LOCTEXT("NoQueryDefinition", "A valid TQS query definition must be specified.");
		return false;
	}
	return true;
}

void UArcTQSQueryDefinitionActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UArcTQSQueryDefinition* Definition = Cast<UArcTQSQueryDefinition>(Asset);
	AArcTQSTestingActor* TestingActor = Cast<AArcTQSTestingActor>(NewActor);
	if (!Definition || !TestingActor)
	{
		return;
	}

	TestingActor->Query.SetDefinition(Definition);
	TestingActor->RunQuery();
}

#undef LOCTEXT_NAMESPACE
