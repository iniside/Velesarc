// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/Navigation/NavigationTypes.h"

class UWorld;

namespace ArcNavHelpers
{
	/** Sync test for path existence from From to To using default nav data.
	 *  Returns false on null world, missing nav system, or no nav data. */
	ARCMASS_API bool PathExists(
		const UWorld* World,
		const FVector& From,
		const FVector& To,
		const FNavAgentProperties& NavAgentProps = FNavAgentProperties::DefaultProperties);

	/** Sync path length in cm. Unset if no path exists or path is invalid. */
	ARCMASS_API TOptional<float> PathLength(
		const UWorld* World,
		const FVector& From,
		const FVector& To,
		const FNavAgentProperties& NavAgentProps = FNavAgentProperties::DefaultProperties);
}
