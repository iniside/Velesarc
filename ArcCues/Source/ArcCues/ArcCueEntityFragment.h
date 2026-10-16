// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"

#include "ArcCueTypes.h"

#include "ArcCueEntityFragment.generated.h"

// ---------------------------------------------------------------------------
// FArcCueEntityFragment
//
// Back-reference fragment placed on cue entities (entities spawned by
// persistent behaviors like FArcCueBehavior_AttachedNiagara). Lets
// UArcCueLostTargetProcessor map a cue entity back to its owning cue when
// the target is lost.
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCCUES_API FArcCueEntityFragment : public FMassFragment
{
	GENERATED_BODY()

	FArcCueHandle OwningCue;
	int32 BehaviorIndex = INDEX_NONE;
};

// FArcCueHandle wraps a single uint32 — trivially copyable. No traits
// specialization needed. (Specializing defensively breaks the static_assert
// only when the type genuinely is non-trivial; keeping this fragment clean
// preserves the project's invariant that traits are added when needed.)
