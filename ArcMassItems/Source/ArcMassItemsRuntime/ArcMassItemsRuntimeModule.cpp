// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsRuntimeModule.h"

#include "Items/ArcItemDefinitionData.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "JsonAssets/ArcJsonRecordSerializer.h"

void FArcMassItemsRuntimeModule::StartupModule()
{
	ArcJsonRecord::RegisterInstancedStructWrapper(*FArcInstancedStruct::StaticStruct(), TEXT("InstancedStruct"),
		[](void* WrapperValue)
		{
			static_cast<FArcInstancedStruct*>(WrapperValue)->UpdateStructName();
		});

	ArcJsonAssets::RegisterType(FArcItemDefinitionData::GetJsonTypeInfo());
}

void FArcMassItemsRuntimeModule::ShutdownModule()
{
	ArcJsonAssets::UnregisterType(FArcItemDefinitionData::JsonExtension());
	ArcJsonRecord::UnregisterInstancedStructWrapper(*FArcInstancedStruct::StaticStruct());
}

IMPLEMENT_MODULE(FArcMassItemsRuntimeModule, ArcMassItemsRuntime)
