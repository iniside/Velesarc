// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

enum class EArcConditionType : uint8;

namespace ArcConditionTags
{
	// Active tags — applied/removed when condition activates/deactivates
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Burning);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Bleeding);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Chilled);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Shocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Poisoned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Diseased);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Weakened);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Oiled);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Wet);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Corroded);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Blinded);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Suffocating);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active_Exhausted);

	// Overload tags — applied/removed when overload begins/ends
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Burning);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Bleeding);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Chilled);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Shocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Poisoned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Diseased);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Weakened);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Oiled);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Wet);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Corroded);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Blinded);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Suffocating);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload_Exhausted);

	// SetByCaller tag for saturation magnitude
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Saturation);

	/** Look up the Active tag for a condition type. */
	ARCCONDITIONEFFECTS_API FGameplayTag GetActiveTag(EArcConditionType Type);

	/** Look up the Overload tag for a condition type. */
	ARCCONDITIONEFFECTS_API FGameplayTag GetOverloadTag(EArcConditionType Type);
}
