// Copyright Lukasz Baran. All Rights Reserved.

#include "Events/ArcEventSubsystemCore.h"

namespace ArcEventSubsystemCore
{

void UnbindPostActorTickDrain(FDelegateHandle& Handle)
{
	if (Handle.IsValid())
	{
		FWorldDelegates::OnWorldPostActorTick.Remove(Handle);
		Handle.Reset();
	}
}

} // namespace ArcEventSubsystemCore
