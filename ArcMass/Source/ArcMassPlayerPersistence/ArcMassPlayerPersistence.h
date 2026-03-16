// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "ArcMassPlayerPersistence.generated.h"

/** Marks a player's Mass entity for player-scoped fragment persistence. */
USTRUCT()
struct ARCMASSPLAYERPERSISTENCE_API FArcMassPlayerPersistenceTag : public FMassTag
{
	GENERATED_BODY()
};
