// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Serialization/ArcPersistenceSerializer.h"

struct FArcItemSpec;

struct ARCCORE_API FArcItemSpecSerializer
{
	using SourceType = FArcItemSpec;
	static constexpr uint32 Version = 1;

	static void Save(const FArcItemSpec& Source, FArcSaveArchive& Ar);
	static void Load(FArcItemSpec& Target, FArcLoadArchive& Ar);
};

UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(FArcItemSpecSerializer, ARCCORE_API);
