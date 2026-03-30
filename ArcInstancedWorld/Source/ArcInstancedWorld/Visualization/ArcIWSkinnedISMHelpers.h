// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassEntityTypes.h"

struct FMassEntityManager;

namespace UE::ArcIW::SkinnedISM
{
	/** Rebuild scene proxies for all dirty skinned ISM holders, then clear their dirty flags.
	 *  Call once per processor after the skinned entity iteration loop. */
	void FlushDirtyHolders(const TSet<FMassEntityHandle>& DirtyHolders, FMassEntityManager& EntityManager);
}
