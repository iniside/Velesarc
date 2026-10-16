// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityHandle.h"
#include "ArcMassAttachmentFragment.generated.h"

// ---------------------------------------------------------------------------
// FArcMassAttachmentFragment
//
// Generic Mass-to-Mass transform attachment primitive. The follower entity's
// FTransformFragment is recomputed each tick as LocalOffset * TargetTransform
// by UArcMassAttachmentProcessor.
//
// Reusable beyond cues: carried items, weapons attached to characters, escort
// entities, vehicle occupants. Sibling to ArcMassPhysicsEntityLink (Mass <->
// Chaos), but on the Mass <-> Mass transform layer.
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcMassAttachmentFragment : public FMassFragment
{
	GENERATED_BODY()

	/** The entity whose transform this follower tracks. */
	FMassEntityHandle Target;

	/** Local-space offset applied relative to Target's world transform. */
	FTransform LocalOffset = FTransform::Identity;
};

// FMassEntityHandle is not guaranteed trivially copyable per project rules.
template<>
struct TMassFragmentTraits<FArcMassAttachmentFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// FArcMassAttachmentLostTag
//
// Sparse marker added by UArcMassAttachmentProcessor when the follower's
// Target handle is no longer valid. Other systems (e.g. cue lost-target
// sweeper) can react to this to perform cleanup.
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcMassAttachmentLostTag : public FMassTag
{
	GENERATED_BODY()
};
