/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"

class FArcSaveArchive;
class FArcLoadArchive;
class UStruct;
class FProperty;

// ─────────────────────────────────────────────────────────────────────────────
// Plan data structures — cached reflection-walk plan (Step 1)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Identifies which branch of the SaveValueDirect dispatch chain handles a property.
 * One value per leaf branch in the current CastField chain, in order of appearance.
 */
enum class EArcPlanEntryKind : uint8
{
	Bool,
	Int32,
	Int64,
	UInt8,
	UInt16,
	UInt32,
	UInt64,
	Float,
	Double,
	String,
	Name,
	Text,
	Enum,
	Guid,
	GameplayTag,
	GameplayTagContainer,
	Vector,
	Rotator,
	Transform,
	PrimaryAssetId,
	SoftObject,
	HardObject,
	RegisteredStruct,
	GenericStruct,
	Array,
	Map,
	Set,

	/** Property type SaveValueDirect does not handle — writes nothing, logs a warning. */
	Unsupported,
};

struct FArcTypePlan;

/**
 * One entry in a cached per-type reflection plan.
 *
 * Fields used depend on Kind:
 *   Enum        — EnumNameKey holds the precomputed "<Key>_Name" debug-string key.
 *   Array/Set   — InnerKind, InnerProp, InnerNestedPlan, ElementKey describe the element.
 *   Map         — additionally MapKeyKind, MapKeyProp describe the key side.
 *   RegisteredStruct / GenericStruct — NestedPlan (may be null for RegisteredStruct).
 *   All others  — unused fields are null/zero.
 */
struct FArcPlanEntry
{
	EArcPlanEntryKind Kind = EArcPlanEntryKind::Bool;

	/** Byte offset of this property inside its owning struct (from Prop->GetOffset_ForInternal()). */
	uint32 Offset = 0;

	/** Archive key for this property (Prop->GetFName()). */
	FName Key;

	/** The reflected property pointer (used for runtime value access). */
	FProperty* Prop = nullptr;

	/** For RegisteredStruct/GenericStruct: cached nested plan for the inner UScriptStruct. */
	const FArcTypePlan* NestedPlan = nullptr;

	/** Enum only: precomputed FName for the "<Key>_Name" debug string written alongside the numeric value. */
	FName EnumNameKey;

	/** Array/Set/Map: the key passed to SaveValueDirect for element recursion (outer Key for Array/Set). */
	FName ElementKey;

	// ── Container inner element ──────────────────────────────────────────────

	/** Array/Set/Map value element: kind of the inner/value element property. */
	EArcPlanEntryKind InnerKind = EArcPlanEntryKind::Bool;

	/** Array/Set/Map value element: the reflected inner/value FProperty. */
	FProperty* InnerProp = nullptr;

	/** Array/Set/Map value element: nested plan if inner element is a struct. */
	const FArcTypePlan* InnerNestedPlan = nullptr;

	// ── Map key element ──────────────────────────────────────────────────────

	/** Map only: kind of the map key property. */
	EArcPlanEntryKind MapKeyKind = EArcPlanEntryKind::Bool;

	/** Map only: the reflected map key FProperty. */
	FProperty* MapKeyProp = nullptr;
};

/**
 * A built-once, reusable serialization plan for a UStruct.
 * Cached per-UStruct* on FArcPersistenceModule for the lifetime of that UStruct.
 */
struct FArcTypePlan
{
	/** Schema version hash at the time the plan was built (from ComputeSchemaVersion). */
	uint32 SchemaVersion = 0;

	/** Flat list of entries — one per SaveGame (or all-) property, in field-iteration order. */
	TArray<FArcPlanEntry> Entries;
};

/**
 * Reflection-based serializer that walks all SaveGame-flagged properties of a UStruct.
 * Used as the fallback when no external serializer is registered for a type.
 *
 * All methods are static — this class is never instantiated.
 */
class ARCPERSISTENCE_API FArcReflectionSerializer
{
public:
	/** Serialize all SaveGame properties of a UStruct instance into the archive. */
	static void Save(const UStruct* Type, const void* Data, FArcSaveArchive& Ar);

	/** Deserialize all SaveGame properties of a UStruct instance from the archive. */
	static void Load(const UStruct* Type, void* Data, FArcLoadArchive& Ar);

	/** Compute a schema version hash from SaveGame property names and types. */
	static uint32 ComputeSchemaVersion(const UStruct* Type);

	/** Serialize ALL properties of a UStruct instance (no SaveGame filter). */
	static void SaveAll(const UStruct* Type, const void* Data, FArcSaveArchive& Ar);

	/** Deserialize ALL properties of a UStruct instance (no SaveGame filter). */
	static void LoadAll(const UStruct* Type, void* Data, FArcLoadArchive& Ar);

	/** Compute a schema version hash from ALL property names and types. */
	static uint32 ComputeSchemaVersionAll(const UStruct* Type);

	// ── Delta (opt-in) — dark-launched; no existing call site wires these in v1 ──

	/**
	 * Serialize only SaveGame properties that differ from DefaultData.
	 * When DefaultData is null this degenerates to a full save (same as Save).
	 * Does NOT modify any existing call site — new callers opt in explicitly.
	 */
	static void SaveDelta(const UStruct* Type, const void* Data, FArcSaveArchive& Ar, const void* DefaultData);

	/**
	 * Serialize only ALL properties that differ from DefaultData.
	 * When DefaultData is null this degenerates to a full save (same as SaveAll).
	 */
	static void SaveDeltaAll(const UStruct* Type, const void* Data, FArcSaveArchive& Ar, const void* DefaultData);

	/**
	 * Serialize only SaveGame properties that differ from the class CDO.
	 * Obtains the CDO default internally; caller need not supply it.
	 */
	static void SaveDeltaFromCDO(const UClass* Class, const void* Data, FArcSaveArchive& Ar);

	/**
	 * Serialize only SaveGame properties that differ from the UScriptStruct default instance.
	 * Uses UScriptStruct::GetDefaultInstance() (or a temporary zeroed instance) as the baseline.
	 */
	static void SaveDeltaFromScriptStructDefault(const UScriptStruct* Struct, const void* Data, FArcSaveArchive& Ar);

#if WITH_EDITOR || !UE_BUILD_SHIPPING
	// ── Test/editor helpers — not available in shipping builds ───────────────

	/** Returns true if a cached plan exists for Type (either SaveGame-only or all-property). */
	static bool HasCachedPlan(const UStruct* Type, bool bSaveGameOnly);

	/** Flushes the entire plan cache. Called on serializer (un)registration to avoid stale entries. */
	static void FlushPlanCache();

	/** Removes the cached plan for a single UStruct (e.g. after Blueprint recompile). */
	static void InvalidatePlan(const UStruct* Type);
#endif // WITH_EDITOR || !UE_BUILD_SHIPPING

private:
	/**
	 * Build (or retrieve from cache) the reflection plan for Type.
	 * If the module is not yet loaded (early/test callers), returns a transient plan without caching.
	 */
	static const FArcTypePlan& GetOrBuildPlan(const UStruct* Type, bool bSaveGameOnly);

	/** Populates an already-reserved (stable-address) plan slot for Type. */
	static void BuildPlanInto(const UStruct* Type, bool bSaveGameOnly, FArcTypePlan& Plan);

	/**
	 * Executes a cached save plan against an instance, writing the same archive-call sequence
	 * (keys + order) as the legacy TFieldIterator/SaveValueDirect path. When DefaultData is
	 * non-null, each top-level entry is skipped (delta) if its value equals the default
	 * (Prop->Identical); when DefaultData is null the gate is inert and output is byte-identical
	 * to the pre-plan path.
	 */
	static void ExecuteSavePlan(const FArcTypePlan& Plan, const void* Data, FArcSaveArchive& Ar, const void* DefaultData);

	/**
	 * Writes a single planned value (leaf, known-struct, RegisteredStruct, or GenericStruct via
	 * nested plan) given a raw value pointer. Used for container element dispatch. If Kind is itself
	 * a container (Array/Map/Set) or Unsupported, falls back to SaveValueDirect for byte-identical
	 * deep-nesting behavior. Container elements never participate in the delta gate.
	 */
	static void ExecuteSaveValue(
		EArcPlanEntryKind Kind,
		FProperty* Prop,
		const FArcTypePlan* NestedPlan,
		FName Key,
		const void* ValuePtr,
		FArcSaveArchive& Ar);

	/**
	 * Resolves a container element (array/set element, map value, map key) into its Kind,
	 * property pointer, and (for generic-struct elements) nested plan. Nested-container
	 * elements record only their container Kind — deeper dispatch falls back to
	 * SaveValueDirect in the executor (planned v1 behavior).
	 */
	static void ResolveElement(
		FProperty* ElementProp,
		EArcPlanEntryKind& OutKind,
		FProperty*& OutProp,
		const FArcTypePlan*& OutNestedPlan);

	/**
	 * Executes a cached load plan against an instance, issuing the same archive-read sequence
	 * (keys + order) as the legacy TFieldIterator/LoadValueDirect path. No DefaultData: a missing
	 * key leaves the destination at its caller-initialized default (ReadProperty returns false and
	 * does not write), which is exactly what makes sparse-delta loads correct with no load-side change.
	 */
	static void ExecuteLoadPlan(const FArcTypePlan& Plan, void* Data, FArcLoadArchive& Ar);

	/**
	 * Reads a single planned value (leaf, known-struct, RegisteredStruct, or GenericStruct via
	 * nested plan) given a raw value pointer. Used for container element dispatch. If Kind is itself
	 * a container (Array/Map/Set) or Unsupported, falls back to LoadValueDirect for byte-identical
	 * deep-nesting behavior.
	 */
	static void ExecuteLoadValue(
		EArcPlanEntryKind Kind,
		FProperty* Prop,
		const FArcTypePlan* NestedPlan,
		FName Key,
		void* ValuePtr,
		FArcLoadArchive& Ar);

	static void SaveProperty(FProperty* Prop, const void* ContainerData, FArcSaveArchive& Ar);
	static void LoadProperty(FProperty* Prop, void* ContainerData, FArcLoadArchive& Ar);

	/**
	 * Save/Load a single value given a raw pointer to the value (not a container).
	 * Used for array element dispatch where ContainerPtrToValuePtr is not applicable.
	 * The Key is used for struct/array nesting context within the current scope.
	 */
	static void SaveValueDirect(FProperty* Prop, FName Key, const void* ValuePtr, FArcSaveArchive& Ar);
	static void LoadValueDirect(FProperty* Prop, FName Key, void* ValuePtr, FArcLoadArchive& Ar);
};
