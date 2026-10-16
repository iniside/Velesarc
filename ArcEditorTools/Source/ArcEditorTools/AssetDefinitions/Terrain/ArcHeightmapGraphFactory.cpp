// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Terrain/ArcHeightmapGraphFactory.h"
#include "TerrainTool/Graph/ArcHeightmapGraph.h"

UArcHeightmapGraphFactory::UArcHeightmapGraphFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcHeightmapGraph::StaticClass();
}

UObject* UArcHeightmapGraphFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcHeightmapGraph>(InParent, Class, Name, Flags);
}
