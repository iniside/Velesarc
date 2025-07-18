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

#include "ArcStaticsBFL.h"

#include "Engine/Engine.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassSpawnerTypes.h"
#include "MassSpawnLocationProcessor.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Core/ArcCoreAssetManager.h"
#include "GameMode/ArcUserFacingExperienceDefinition.h"
#include "Kismet/GameplayStatics.h"

void UArcStaticsBFL::OpenLevelByPrimaryAssetId(const UObject* WorldContextObject
											   , const FPrimaryAssetId Level
											   , bool bAbsolute
											   , FString Options)
{
	UArcCoreAssetManager& AM = UArcCoreAssetManager::Get();
	FSoftObjectPath Path = AM.GetPrimaryAssetPath(Level);
	const FName LevelName = FName(*FPackageName::ObjectPathToPackageName(Path.ToString()));
	UGameplayStatics::OpenLevel(WorldContextObject
		, LevelName
		, bAbsolute
		, Options);
}

void UArcStaticsBFL::OpenLevelFromUserFacingExperience(const UObject* WorldContextObject, UArcUserFacingExperienceDefinition* InDefinition)
{
	UArcCoreAssetManager& AM = UArcCoreAssetManager::Get();

	FSoftObjectPath Path = AM.GetPrimaryAssetPath(InDefinition->MapID);
	const FName LevelName = FName(*FPackageName::ObjectPathToPackageName(Path.ToString()));

	FString Options;
	Options += "UserExperience=";
	Options += InDefinition->ExperienceID.AssetId.ToString();
	
	UGameplayStatics::OpenLevel(WorldContextObject
		, LevelName
		, true
		, "");
}

void UArcStaticsBFL::SpawnSingleEntity(UObject* ContextObject, UMassEntityConfigAsset* InConfig, FVector InLocation)
{
	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(ContextObject->GetWorld());

	const FMassEntityTemplate& Template =  InConfig->GetOrCreateEntityTemplate(*ContextObject->GetWorld());
	FInstancedStruct SpawnData;
	SpawnData.InitializeAs<FMassTransformsSpawnData>();
	FMassTransformsSpawnData& Transforms = SpawnData.GetMutable<FMassTransformsSpawnData>();
	Transforms.Transforms.Add(FTransform(InLocation));
	
	TArray<FMassEntityHandle> OutHandles;
	SpawnerSystem->SpawnEntities(Template.GetTemplateID(), 1, SpawnData, UMassSpawnLocationProcessor::StaticClass(), OutHandles);
}

void UArcStaticsBFL::SendCommandToServer(UObject* ContextObject, UScriptStruct* InCommandType, const int32& InCommand)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UArcStaticsBFL::execSendCommandToServer)
{

	P_GET_OBJECT(UObject, ContextObject);
	P_GET_OBJECT(UScriptStruct, InCommandType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;
	bool bSuccess = true;

	{
		P_NATIVE_BEGIN;
		void* Allocated = FMemory::Malloc(InCommandType->GetCppStructOps()->GetSize()
				, InCommandType->GetCppStructOps()->GetAlignment());
		InCommandType->GetCppStructOps()->Construct(Allocated);

		InCommandType->CopyScriptStruct(Allocated, OutItemDataPtr);

		FArcReplicatedCommand* Ptr = static_cast<FArcReplicatedCommand*>(Allocated);
		
		Arcx::SendServerCommand(ContextObject, MoveTemp(Ptr));

		Allocated = nullptr;
		
		P_NATIVE_END;
	}
}

UObject* UArcStaticsBFL::IsObjectChildOf(UObject* InObject
	, TSubclassOf<UObject> InClass
	, bool& bSuccess)
{
	if (!InObject)
	{
		bSuccess = false;
		return nullptr;
	}
	
	if (InObject->GetClass()->IsChildOf(InClass))
	{
		bSuccess = true;
		return InObject;
	}

	bSuccess = false;
	return nullptr;
}
