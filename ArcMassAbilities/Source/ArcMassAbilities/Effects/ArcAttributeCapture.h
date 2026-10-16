// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Attributes/ArcAttribute.h"
#include "ArcAttributeCapture.generated.h"

UENUM(BlueprintType)
enum class EArcCaptureSource : uint8
{
	Source,
	Target
};

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcAttributeCaptureDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere)
	EArcCaptureSource CaptureSource = EArcCaptureSource::Source;

	UPROPERTY(EditAnywhere)
	bool bSnapshot = true;

	bool operator==(const FArcAttributeCaptureDef& Other) const;
	friend ARCMASSABILITIES_API uint32 GetTypeHash(const FArcAttributeCaptureDef& Def);
};

struct ARCMASSABILITIES_API FArcCapturedAttribute
{
	FArcAttributeCaptureDef Definition;
	float SnapshotValue = 0.f;
	bool bHasSnapshot = false;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEvaluateParameters
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTagContainer SourceTags;

	UPROPERTY()
	FGameplayTagContainer TargetTags;
};

namespace Arc::Capture   // CLAUDE.md: free functions live in a named namespace
{
	/** Builds a capture def from an attribute getter + policy. Getter is a non-type template
	 *  parameter (requires C++17 — UE5/MSVC compiles /std:c++17+) so each (getter, source,
	 *  snapshot) triple is a distinct instantiation. Returns by value; the ARC_DECLARE_CAPTURE_*
	 *  accessors cache the result in a function-local `static const` and hand out a const ref. */
	template <FArcAttributeRef (*AttrGetter)(), EArcCaptureSource InSource, bool bInSnapshot>
	FArcAttributeCaptureDef MakeCaptureDef()
	{
		FArcAttributeCaptureDef Def;
		Def.Attribute     = AttrGetter();
		Def.CaptureSource = InSource;
		Def.bSnapshot     = bInSnapshot;
		return Def;
	}

	inline FArcAttributeCaptureDef MakeRuntimeCaptureDef(FArcAttributeRef Attribute, EArcCaptureSource Source, bool bSnapshot)
	{
		FArcAttributeCaptureDef Def;
		Def.Attribute     = MoveTemp(Attribute);
		Def.CaptureSource = Source;
		Def.bSnapshot     = bSnapshot;
		return Def;
	}
}

// --- Declaration macros: mint a named static accessor on an attribute set ---------------------
// Pairs with ARC_MASS_ATTRIBUTE_ACCESSORS(Owner, Attr) (which provides Get##Attr##Attribute()).
// Call the namespaced helper fully-qualified so the macros work from any namespace context.
//
// The def is invariant (fully determined by the template params), so each accessor computes it once
// into a function-local `static const` and returns it by const ref. This is collision-free *because*
// the getter is a non-type template param: each accessor is its own instantiation, so its static is
// one-per-(Owner,Attr,policy). (The static-cache trap only bites the rejected runtime-arg form, where
// many getters would share one <Source,Snap> instantiation.) FArcAttributeCaptureDef is safe to hold
// for process lifetime — FArcAttributeRef (FName + UScriptStruct*), an enum, a bool. Read-only
// consumers (CalcCapturedMagnitude) bind the ref directly; Captures.Add still copies once into the
// by-value array, which is unavoidable.
#define ARC_DECLARE_CAPTURE_SOURCE_SNAPSHOT(Owner, Attr) \
	static const FArcAttributeCaptureDef& Get##Attr##SourceSnapshotCaptureDef() \
	{ static const FArcAttributeCaptureDef Def = Arc::Capture::MakeCaptureDef<&Owner::Get##Attr##Attribute, EArcCaptureSource::Source, true>(); return Def; }
#define ARC_DECLARE_CAPTURE_SOURCE_LIVE(Owner, Attr) \
	static const FArcAttributeCaptureDef& Get##Attr##SourceLiveCaptureDef() \
	{ static const FArcAttributeCaptureDef Def = Arc::Capture::MakeCaptureDef<&Owner::Get##Attr##Attribute, EArcCaptureSource::Source, false>(); return Def; }
#define ARC_DECLARE_CAPTURE_TARGET_SNAPSHOT(Owner, Attr) \
	static const FArcAttributeCaptureDef& Get##Attr##TargetSnapshotCaptureDef() \
	{ static const FArcAttributeCaptureDef Def = Arc::Capture::MakeCaptureDef<&Owner::Get##Attr##Attribute, EArcCaptureSource::Target, true>(); return Def; }
#define ARC_DECLARE_CAPTURE_TARGET_LIVE(Owner, Attr) \
	static const FArcAttributeCaptureDef& Get##Attr##TargetLiveCaptureDef() \
	{ static const FArcAttributeCaptureDef Def = Arc::Capture::MakeCaptureDef<&Owner::Get##Attr##Attribute, EArcCaptureSource::Target, false>(); return Def; }

// --- Getter macros: terse call-site sugar forwarding to the named accessor --------------------
#define GET_CAPTURE_SOURCE_SNAPSHOT(Owner, Attr) Owner::Get##Attr##SourceSnapshotCaptureDef()
#define GET_CAPTURE_SOURCE_LIVE(Owner, Attr)     Owner::Get##Attr##SourceLiveCaptureDef()
#define GET_CAPTURE_TARGET_SNAPSHOT(Owner, Attr) Owner::Get##Attr##TargetSnapshotCaptureDef()
#define GET_CAPTURE_TARGET_LIVE(Owner, Attr)     Owner::Get##Attr##TargetLiveCaptureDef()
