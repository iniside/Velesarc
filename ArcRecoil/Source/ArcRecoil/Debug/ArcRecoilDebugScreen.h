// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ArcRecoil::DebugScreen
{
	const TCHAR* ShowFlagName();

	/** False means UDebugDrawService rejected the show flag name and the overlay can never draw. */
	bool Register();

	void Unregister();
}
