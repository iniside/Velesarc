// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArcPersistenceSerializer.h"
#include "ArcPlayerProviderDescriptor.h"

class UMassAgentComponent;
class FArcSaveArchive;
class FArcLoadArchive;

/**
 * Persistence serializer for player Mass fragments.
 * Resolves the entity handle from UMassAgentComponent, then
 * delegates to FArcMassFragmentSerializer for actual fragment I/O.
 */
struct ARCMASSPLAYERPERSISTENCE_API FArcPlayerMassFragmentSerializer
{
	using SourceType = UMassAgentComponent;
	static constexpr uint32 Version = 1;

	static void Save(const UMassAgentComponent& Source, FArcSaveArchive& Ar);
	static void Load(UMassAgentComponent& Target, FArcLoadArchive& Ar);
};

UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(FArcPlayerMassFragmentSerializer, ARCMASSPLAYERPERSISTENCE_API);
UE_ARC_DECLARE_PLAYER_PROVIDER(UMassAgentComponent, ARCMASSPLAYERPERSISTENCE_API);
