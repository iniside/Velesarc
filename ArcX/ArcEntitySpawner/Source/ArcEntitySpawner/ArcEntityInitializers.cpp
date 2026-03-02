// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntityInitializers.h"

#include "MassEntityManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEntityInitializers)

//----------------------------------------------------------------------
// UArcSetFragmentValuesInitializer
//----------------------------------------------------------------------

void UArcSetFragmentValuesInitializer::InitializeEntities(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities)
{
	for (const FMassEntityHandle Entity : Entities)
	{
		for (const FInstancedStruct& FragmentValue : FragmentValues)
		{
			if (!FragmentValue.IsValid())
			{
				continue;
			}

			const UScriptStruct* FragmentType = FragmentValue.GetScriptStruct();
			FStructView FragmentView = EntityManager.GetFragmentDataStruct(Entity, FragmentType);
			if (FragmentView.IsValid())
			{
				FragmentType->CopyScriptStruct(FragmentView.GetMemory(), FragmentValue.GetMemory());
			}
		}
	}
}

//----------------------------------------------------------------------
// UArcAddTagsInitializer
//----------------------------------------------------------------------

void UArcAddTagsInitializer::InitializeEntities(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities)
{
	for (const FMassEntityHandle Entity : Entities)
	{
		for (const FInstancedStruct& Tag : Tags)
		{
			if (const UScriptStruct* TagType = Tag.GetScriptStruct())
			{
				EntityManager.AddTagToEntity(Entity, TagType);
			}
		}
	}
}
