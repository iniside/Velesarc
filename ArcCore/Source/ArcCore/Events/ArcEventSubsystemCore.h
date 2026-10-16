// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"

namespace ArcEventSubsystemCore
{
	/** Find and remove Handle from a per-key TMap<Key, MulticastDelegateT> where the key IS the
	 *  lookup value (direct-key maps like TMap<FMassEntityHandle, FOnXxx>).
	 *  Returns true if a delegate was removed. Prunes empty entries to keep the map from growing
	 *  without bound.
	 *
	 *  Note: this helper is for direct-key maps only. Maps with composite keys (e.g.
	 *  TMap<FArcItemEventKey, TDelegate>) where the entity is one field of a compound key require
	 *  a scan-style helper and cannot be folded here without introducing domain-specific types. */
	template <typename MapType>
	bool TryRemoveHandle(MapType& Map, const typename MapType::KeyType& Key, FDelegateHandle Handle)
	{
		typename MapType::ValueType* Delegate = Map.Find(Key);
		if (Delegate == nullptr)
		{
			return false;
		}
		if (Delegate->Remove(Handle))
		{
			if (!Delegate->IsBound())
			{
				Map.Remove(Key);
			}
			return true;
		}
		return false;
	}

	/** Bind a subsystem's DrainOnPostActorTick(UWorld*, ELevelTick, float) callback to
	 *  FWorldDelegates::OnWorldPostActorTick via AddUObject. The caller stores the returned
	 *  handle and passes it to UnbindPostActorTickDrain in Deinitialize(). */
	template <typename TSubsystem>
	FDelegateHandle BindPostActorTickDrain(TSubsystem* Subsystem)
	{
		return FWorldDelegates::OnWorldPostActorTick.AddUObject(
			Subsystem, &TSubsystem::DrainOnPostActorTick);
	}

	/** Remove a previously bound PostActorTick callback. Safe to call with an invalid handle.
	 *  Resets the handle to the invalid state on exit. */
	ARCCORE_API void UnbindPostActorTickDrain(FDelegateHandle& Handle);

} // namespace ArcEventSubsystemCore
