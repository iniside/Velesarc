// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "UObject/PrimaryAssetId.h"

struct FArcMassItemStoreFragment;
struct FMassEntityManager;
class UScriptStruct;

namespace ArcBodyItemTransfer
{
	ARCWORLDSIM_API int32 TransferItems(
		FMassEntityManager& EntityManager,
		FMassEntityHandle From,
		const FArcMassItemStoreFragment& FromStore,
		const UScriptStruct* FromStoreType,
		FMassEntityHandle To,
		const UScriptStruct* ToStoreType,
		const FPrimaryAssetId& Item,
		int32 MaxAmount);

	ARCWORLDSIM_API int32 Pickup(
		FMassEntityManager& EntityManager,
		FMassEntityHandle User,
		FMassEntityHandle Source,
		const FPrimaryAssetId& Item,
		int32 MaxAmount);
	ARCWORLDSIM_API int32 Drop(
		FMassEntityManager& EntityManager,
		FMassEntityHandle User,
		FMassEntityHandle Sink,
		const FPrimaryAssetId& Item,
		int32 MaxAmount);
}
