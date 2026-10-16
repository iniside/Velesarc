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

#include "Serialization/ArcReflectionSerializer.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"

#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/SoftObjectPtr.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "Serialization/ArcSerializerRegistry.h"
#include "ArcPersistenceModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcReflectionSerializer, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Plan cache accessor — fetches the module-owned cache (null when not loaded)
// ─────────────────────────────────────────────────────────────────────────────

namespace ArcReflectionSerializerPlanCache_Internal
{
	/**
	 * Returns the module-owned plan cache, or nullptr if the module is not loaded.
	 * Not a function-static: the cache lives on the module so it is torn down with the
	 * module (no destruction-order hazard). Callers must treat null as "no cache" and
	 * fall back to a transient build (handled in Step 3).
	 */
	static FArcTypePlanCache* GetPlanCache()
	{
		FArcPersistenceModule* Module =
			FModuleManager::GetModulePtr<FArcPersistenceModule>("ArcPersistence");
		return Module != nullptr ? &Module->PlanCache : nullptr;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Cache lifetime helpers — flush / invalidate / query (Step 2)
// ─────────────────────────────────────────────────────────────────────────────

#if WITH_EDITOR || !UE_BUILD_SHIPPING
void FArcReflectionSerializer::FlushPlanCache()
{
	check(IsInGameThread());
	if (FArcTypePlanCache* Cache = ArcReflectionSerializerPlanCache_Internal::GetPlanCache())
	{
		Cache->SaveGamePlans.Empty();
		Cache->AllPropertyPlans.Empty();
	}
}

void FArcReflectionSerializer::InvalidatePlan(const UStruct* Type)
{
	check(IsInGameThread());
	if (FArcTypePlanCache* Cache = ArcReflectionSerializerPlanCache_Internal::GetPlanCache())
	{
		Cache->SaveGamePlans.Remove(Type);
		Cache->AllPropertyPlans.Remove(Type);
	}
}

bool FArcReflectionSerializer::HasCachedPlan(const UStruct* Type, bool bSaveGameOnly)
{
	check(IsInGameThread());
	const FArcTypePlanCache* Cache = ArcReflectionSerializerPlanCache_Internal::GetPlanCache();
	if (Cache == nullptr)
	{
		return false;
	}
	const TMap<const UStruct*, TUniquePtr<FArcTypePlan>>& Map =
		bSaveGameOnly ? Cache->SaveGamePlans : Cache->AllPropertyPlans;
	return Map.Contains(Type);
}
#endif // WITH_EDITOR || !UE_BUILD_SHIPPING

// ─────────────────────────────────────────────────────────────────────────────
// Plan builder helpers (Step 3) — file-scoped unique namespace
// ─────────────────────────────────────────────────────────────────────────────

namespace ArcReflectionSerializerBuilder_Internal
{
	/**
	 * Computes the schema-version hash for Type. Byte-identical to the loop bodies of
	 * FArcReflectionSerializer::ComputeSchemaVersion (bSaveGameOnly) and
	 * ComputeSchemaVersionAll (!bSaveGameOnly) — copied verbatim so the cached plan and
	 * (Step 6) ComputeSchemaVersion* produce the same value. Top-level members only,
	 * matching the pre-existing limitation.
	 */
	static uint32 HashSchema(const UStruct* Type, bool bSaveGameOnly)
	{
		uint32 Hash = 0;
		for (TFieldIterator<FProperty> It(Type); It; ++It)
		{
			if (bSaveGameOnly && !(*It)->HasAnyPropertyFlags(CPF_SaveGame))
			{
				continue;
			}
			Hash = HashCombine(Hash, GetTypeHash((*It)->GetFName()));
			Hash = HashCombine(Hash, GetTypeHash(FName((*It)->GetCPPType())));
		}
		return Hash;
	}

	/**
	 * Resolves the EArcPlanEntryKind for a single property by walking the SAME branch
	 * sequence as FArcReflectionSerializer::SaveValueDirect, in the SAME ORDER. Byte-identity
	 * of the dispatch depends on this ordering — in particular FSoftObjectProperty is tested
	 * BEFORE FObjectPropertyBase (soft is-a object base).
	 *
	 * Containers (Array/Map/Set) return their container Kind here; element/key dispatch is
	 * resolved separately by the caller (it needs to recurse for nested struct plans).
	 */
	static EArcPlanEntryKind ResolveKind(const FProperty* Prop)
	{
		// Mirror SaveValueDirect's CastField chain order exactly.
		if (CastField<FBoolProperty>(Prop))
		{
			return EArcPlanEntryKind::Bool;
		}
		if (CastField<FIntProperty>(Prop))
		{
			return EArcPlanEntryKind::Int32;
		}
		if (CastField<FInt64Property>(Prop))
		{
			return EArcPlanEntryKind::Int64;
		}
		if (CastField<FByteProperty>(Prop))
		{
			return EArcPlanEntryKind::UInt8;
		}
		if (CastField<FUInt16Property>(Prop))
		{
			return EArcPlanEntryKind::UInt16;
		}
		if (CastField<FUInt32Property>(Prop))
		{
			return EArcPlanEntryKind::UInt32;
		}
		if (CastField<FUInt64Property>(Prop))
		{
			return EArcPlanEntryKind::UInt64;
		}
		if (CastField<FFloatProperty>(Prop))
		{
			return EArcPlanEntryKind::Float;
		}
		if (CastField<FDoubleProperty>(Prop))
		{
			return EArcPlanEntryKind::Double;
		}
		if (CastField<FStrProperty>(Prop))
		{
			return EArcPlanEntryKind::String;
		}
		if (CastField<FNameProperty>(Prop))
		{
			return EArcPlanEntryKind::Name;
		}
		if (CastField<FTextProperty>(Prop))
		{
			return EArcPlanEntryKind::Text;
		}
		if (CastField<FEnumProperty>(Prop))
		{
			return EArcPlanEntryKind::Enum;
		}

		// ── Struct: known names first, then registry, then generic ──────────
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
		{
			const UScriptStruct* Struct = StructProp->Struct;
			const FName StructFName = Struct->GetFName();

			static const FName NAME_FGuid(TEXT("Guid"));
			static const FName NAME_GameplayTag(TEXT("GameplayTag"));
			static const FName NAME_GameplayTagContainer(TEXT("GameplayTagContainer"));
			static const FName NAME_PrimaryAssetId(TEXT("PrimaryAssetId"));

			if (StructFName == NAME_FGuid)
			{
				return EArcPlanEntryKind::Guid;
			}
			if (StructFName == NAME_GameplayTag)
			{
				return EArcPlanEntryKind::GameplayTag;
			}
			if (StructFName == NAME_GameplayTagContainer)
			{
				return EArcPlanEntryKind::GameplayTagContainer;
			}
			if (StructFName == NAME_Vector)
			{
				return EArcPlanEntryKind::Vector;
			}
			if (StructFName == NAME_Rotator)
			{
				return EArcPlanEntryKind::Rotator;
			}
			if (StructFName == NAME_Transform)
			{
				return EArcPlanEntryKind::Transform;
			}
			if (StructFName == NAME_PrimaryAssetId)
			{
				return EArcPlanEntryKind::PrimaryAssetId;
			}

			// Registry lookup → RegisteredStruct, else generic struct recursion.
			if (FArcSerializerRegistry::Get().Find(Struct) != nullptr)
			{
				return EArcPlanEntryKind::RegisteredStruct;
			}
			return EArcPlanEntryKind::GenericStruct;
		}

		if (CastField<FArrayProperty>(Prop))
		{
			return EArcPlanEntryKind::Array;
		}
		if (CastField<FMapProperty>(Prop))
		{
			return EArcPlanEntryKind::Map;
		}
		if (CastField<FSetProperty>(Prop))
		{
			return EArcPlanEntryKind::Set;
		}

		// CRITICAL: soft-object is-a object base, so it must be tested before HardObject.
		if (CastField<FSoftObjectProperty>(Prop))
		{
			return EArcPlanEntryKind::SoftObject;
		}
		if (CastField<FObjectPropertyBase>(Prop))
		{
			return EArcPlanEntryKind::HardObject;
		}

		// Unsupported type — SaveValueDirect logs a warning and writes nothing. Reported as
		// the dedicated Unsupported kind so it is never confused with a genuine Bool. The
		// entry IS recorded (Prop carried through) so the executor (Step 4) can reproduce
		// the warning; it produces no archive output, preserving byte-identity.
		return EArcPlanEntryKind::Unsupported;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// GetOrBuildPlan (Step 3) — build (or fetch cached) the per-type reflection plan
// ─────────────────────────────────────────────────────────────────────────────

const FArcTypePlan& FArcReflectionSerializer::GetOrBuildPlan(const UStruct* Type, bool bSaveGameOnly)
{
	check(IsInGameThread());

	using namespace ArcReflectionSerializerBuilder_Internal;

	// Debug guard: catch (illegal) circular value-structs that would recurse forever.
	// A struct that contains itself by value is impossible in UE reflection, but a
	// build cycle would also indicate a logic error in nested-plan recursion.
	static thread_local TSet<const UStruct*> GBuildingTypes;
	check(!GBuildingTypes.Contains(Type));

	// Transient storage for the pre-startup / test path where the module-owned cache is
	// unavailable. Plans are leaked into this list (kept alive for process lifetime) so
	// the returned reference and any NestedPlan pointers into it stay valid. This path is
	// rare (pre-StartupModule callers only).
	static TArray<TUniquePtr<FArcTypePlan>> GTransientPlans;

	FArcTypePlanCache* Cache = ArcReflectionSerializerPlanCache_Internal::GetPlanCache();

	// ── Cached path ─────────────────────────────────────────────────────────
	if (Cache != nullptr)
	{
		TMap<const UStruct*, TUniquePtr<FArcTypePlan>>& Map =
			bSaveGameOnly ? Cache->SaveGamePlans : Cache->AllPropertyPlans;

		if (const TUniquePtr<FArcTypePlan>* Existing = Map.Find(Type))
		{
			return **Existing;
		}

		// Reserve a stable slot FIRST. The plan's storage is the TUniquePtr's heap block,
		// so recursive GetOrBuildPlan calls (which may rehash Map) never move this plan.
		FArcTypePlan& Plan = *Map.Emplace(Type, MakeUnique<FArcTypePlan>());

		GBuildingTypes.Add(Type);
		BuildPlanInto(Type, bSaveGameOnly, Plan);
		GBuildingTypes.Remove(Type);

		return Plan;
	}

	// ── Transient (uncached) path ─────────────────────────────────────────────
	TUniquePtr<FArcTypePlan>& Slot = GTransientPlans.Emplace_GetRef(MakeUnique<FArcTypePlan>());
	FArcTypePlan& Plan = *Slot;

	GBuildingTypes.Add(Type);
	BuildPlanInto(Type, bSaveGameOnly, Plan);
	GBuildingTypes.Remove(Type);

	return Plan;
}

// ─────────────────────────────────────────────────────────────────────────────
// BuildPlanInto (Step 3) — populate an already-reserved plan slot for Type
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::BuildPlanInto(const UStruct* Type, bool bSaveGameOnly, FArcTypePlan& Plan)
{
	using namespace ArcReflectionSerializerBuilder_Internal;

	Plan.SchemaVersion = HashSchema(Type, bSaveGameOnly);
	Plan.Entries.Reset();

	// Iterate with default flags → base→derived, declaration order, matching the old
	// TFieldIterator walk in Save/SaveAll exactly. Apply the SaveGame filter only when asked.
	for (TFieldIterator<FProperty> It(Type); It; ++It)
	{
		FProperty* Prop = *It;
		if (bSaveGameOnly && !Prop->HasAnyPropertyFlags(CPF_SaveGame))
		{
			continue;
		}

		const EArcPlanEntryKind Kind = ResolveKind(Prop);

		FArcPlanEntry Entry;
		Entry.Kind = Kind;
		Entry.Offset = static_cast<uint32>(Prop->GetOffset_ForInternal());
		Entry.Key = Prop->GetFName();
		Entry.Prop = Prop;

		switch (Kind)
		{
		case EArcPlanEntryKind::Enum:
		{
			// Precompute the "<Key>_Name" debug-string key exactly as SaveValueDirect
			// builds it (line ~220) — preserves byte-identity, removes the per-save alloc.
			Entry.EnumNameKey = FName(*FString::Printf(TEXT("%s_Name"), *Entry.Key.ToString()));
			break;
		}
		case EArcPlanEntryKind::GenericStruct:
		{
			// SaveValueDirect's generic-struct branch calls the SaveGame-FILTERED public
			// Save (line ~294) UNCONDITIONALLY — even under SaveAll. So nested plans are
			// always built SaveGame-only, never propagating the outer flag.
			const FStructProperty* StructProp = CastField<FStructProperty>(Prop);
			Entry.NestedPlan = &GetOrBuildPlan(StructProp->Struct, /*bSaveGameOnly=*/true);
			break;
		}
		case EArcPlanEntryKind::RegisteredStruct:
		{
			// RegisteredStruct uses the registry's SaveFunc at execution time (re-looked-up
			// then, so it survives re-registration). No NestedPlan.
			break;
		}
		case EArcPlanEntryKind::Array:
		{
			const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
			// Array elements recurse with the OUTER property Key (SaveValueDirect line ~317).
			Entry.ElementKey = Entry.Key;
			ResolveElement(ArrayProp->Inner, Entry.InnerKind, Entry.InnerProp, Entry.InnerNestedPlan);
			break;
		}
		case EArcPlanEntryKind::Set:
		{
			const FSetProperty* SetProp = CastField<FSetProperty>(Prop);
			// Set elements recurse with the OUTER property Key (SaveValueDirect line ~375).
			Entry.ElementKey = Entry.Key;
			ResolveElement(SetProp->ElementProp, Entry.InnerKind, Entry.InnerProp, Entry.InnerNestedPlan);
			break;
		}
		case EArcPlanEntryKind::Map:
		{
			const FMapProperty* MapProp = CastField<FMapProperty>(Prop);
			// Map key side uses literal "k", value side uses literal "v" (SaveValueDirect
			// lines ~346-347). ElementKey carries "v" for the value; "k" is reproduced at exec.
			Entry.ElementKey = FName("v");
			ResolveElement(MapProp->ValueProp, Entry.InnerKind, Entry.InnerProp, Entry.InnerNestedPlan);

			// The map key element needs no nested plan: the planned v1 dispatch handles map
			// keys via the executor's SaveValueDirect fallback for non-scalar keys, but we
			// still record its resolved Kind/Prop for the common scalar-key case.
			const FArcTypePlan* UnusedKeyNested = nullptr;
			ResolveElement(MapProp->KeyProp, Entry.MapKeyKind, Entry.MapKeyProp, UnusedKeyNested);
			break;
		}
		default:
			break;
		}

		Plan.Entries.Add(MoveTemp(Entry));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ResolveElement (Step 3) — resolve a container element (array/set element, map value/key)
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::ResolveElement(
	FProperty* ElementProp,
	EArcPlanEntryKind& OutKind,
	FProperty*& OutProp,
	const FArcTypePlan*& OutNestedPlan)
{
	using namespace ArcReflectionSerializerBuilder_Internal;

	OutProp = ElementProp;
	OutNestedPlan = nullptr;
	OutKind = ResolveKind(ElementProp);

	// If the element is itself a generic struct, build its nested plan (always SaveGame-only,
	// matching the unconditional filtered Save inside SaveValueDirect's generic-struct branch).
	if (OutKind == EArcPlanEntryKind::GenericStruct)
	{
		const FStructProperty* ElemStructProp = CastField<FStructProperty>(ElementProp);
		OutNestedPlan = &GetOrBuildPlan(ElemStructProp->Struct, /*bSaveGameOnly=*/true);
	}
	// If the element is itself a container (TMap<K, TArray<V>> etc.), record only its
	// container Kind + Prop here. Deeper levels are handled by the executor's SaveValueDirect
	// fallback in Steps 4-5 — this is the planned v1 behavior (per-element nested-container
	// dispatch is not unrolled into the plan).
}

// ─────────────────────────────────────────────────────────────────────────────
// ExecuteSaveValue (Step 4) — write one planned value given a raw value pointer
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::ExecuteSaveValue(
	EArcPlanEntryKind Kind,
	FProperty* Prop,
	const FArcTypePlan* NestedPlan,
	FName Key,
	const void* ValuePtr,
	FArcSaveArchive& Ar)
{
	switch (Kind)
	{
	case EArcPlanEntryKind::Bool:
	{
		const FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(Prop);
		Ar.WriteProperty(Key, BoolProp->GetPropertyValue(ValuePtr));
		return;
	}
	case EArcPlanEntryKind::Int32:
		Ar.WriteProperty(Key, *static_cast<const int32*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Int64:
		Ar.WriteProperty(Key, *static_cast<const int64*>(ValuePtr));
		return;
	case EArcPlanEntryKind::UInt8:
		Ar.WriteProperty(Key, *static_cast<const uint8*>(ValuePtr));
		return;
	case EArcPlanEntryKind::UInt16:
		Ar.WriteProperty(Key, *static_cast<const uint16*>(ValuePtr));
		return;
	case EArcPlanEntryKind::UInt32:
		Ar.WriteProperty(Key, *static_cast<const uint32*>(ValuePtr));
		return;
	case EArcPlanEntryKind::UInt64:
		Ar.WriteProperty(Key, *static_cast<const uint64*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Float:
		Ar.WriteProperty(Key, *static_cast<const float*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Double:
		Ar.WriteProperty(Key, *static_cast<const double*>(ValuePtr));
		return;
	case EArcPlanEntryKind::String:
		Ar.WriteProperty(Key, *static_cast<const FString*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Name:
		Ar.WriteProperty(Key, *static_cast<const FName*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Text:
		Ar.WriteProperty(Key, *static_cast<const FText*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Enum:
	{
		// NOTE: this branch is only reached for container ELEMENTS (where the planner does not
		// precompute an EnumNameKey). It mirrors SaveValueDirect's enum branch but rebuilds the
		// "<Key>_Name" key from the supplied element Key — byte-identical to the legacy path,
		// which likewise rebuilt it per element. Top-level enum entries use the precomputed
		// EnumNameKey via ExecuteSavePlan and never reach here.
		const FEnumProperty* EnumProp = CastFieldChecked<FEnumProperty>(Prop);
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		const int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		Ar.WriteProperty(Key, EnumValue);

		const FName EnumNameKey(*FString::Printf(TEXT("%s_Name"), *Key.ToString()));
		const UEnum* Enum = EnumProp->GetEnum();
		if (Enum)
		{
			const FString EnumValueName = Enum->GetNameStringByValue(EnumValue);
			Ar.WriteProperty(EnumNameKey, EnumValueName);
		}
		return;
	}
	case EArcPlanEntryKind::Guid:
		Ar.WriteProperty(Key, *static_cast<const FGuid*>(ValuePtr));
		return;
	case EArcPlanEntryKind::GameplayTag:
		Ar.WriteProperty(Key, *static_cast<const FGameplayTag*>(ValuePtr));
		return;
	case EArcPlanEntryKind::GameplayTagContainer:
		Ar.WriteProperty(Key, *static_cast<const FGameplayTagContainer*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Vector:
		Ar.WriteProperty(Key, *static_cast<const FVector*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Rotator:
		Ar.WriteProperty(Key, *static_cast<const FRotator*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Transform:
		Ar.WriteProperty(Key, *static_cast<const FTransform*>(ValuePtr));
		return;
	case EArcPlanEntryKind::PrimaryAssetId:
	{
		const FPrimaryAssetId& Id = *static_cast<const FPrimaryAssetId*>(ValuePtr);
		Ar.WriteProperty(Key, Id.ToString());
		return;
	}
	case EArcPlanEntryKind::SoftObject:
	{
		const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
		Ar.WriteProperty(Key, SoftPtr.ToString());
		return;
	}
	case EArcPlanEntryKind::HardObject:
	{
		const FObjectPropertyBase* ObjProp = CastFieldChecked<FObjectPropertyBase>(Prop);
		const UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
		if (Obj)
		{
			UE_LOG(LogArcReflectionSerializer, Warning,
				TEXT("Serializing hard object reference for property '%s' (%s) — consider using TSoftObjectPtr for persistence."),
				*Prop->GetName(), *Obj->GetPathName());
			const FSoftObjectPath SoftPath(Obj);
			Ar.WriteProperty(Key, SoftPath.ToString());
		}
		else
		{
			Ar.WriteProperty(Key, FString());
		}
		return;
	}
	case EArcPlanEntryKind::RegisteredStruct:
	{
		// Re-look-up the registry at execution time so the plan survives re-registration.
		const FStructProperty* StructProp = CastFieldChecked<FStructProperty>(Prop);
		const FArcPersistenceSerializerInfo* RegisteredInfo =
			FArcSerializerRegistry::Get().Find(StructProp->Struct);
		if (RegisteredInfo)
		{
			Ar.BeginStruct(Key);
			RegisteredInfo->SaveFunc(ValuePtr, Ar);
			Ar.EndStruct();
		}
		else
		{
			// Registration was removed since the plan was built — fall back to a generic
			// struct recurse so output stays well-formed (matches SaveValueDirect's behavior
			// when no registered serializer exists).
			Ar.BeginStruct(Key);
			Save(StructProp->Struct, ValuePtr, Ar);
			Ar.EndStruct();
		}
		return;
	}
	case EArcPlanEntryKind::GenericStruct:
	{
		check(NestedPlan != nullptr);
		Ar.BeginStruct(Key);
		ExecuteSavePlan(*NestedPlan, ValuePtr, Ar, /*DefaultData=*/nullptr);
		Ar.EndStruct();
		return;
	}
	case EArcPlanEntryKind::Array:
	case EArcPlanEntryKind::Map:
	case EArcPlanEntryKind::Set:
	case EArcPlanEntryKind::Unsupported:
	default:
		// Container element is itself a container, or an unsupported type — fall back to the
		// legacy recursive writer. Byte-identical for the deep-nesting case (planned v1 behavior).
		SaveValueDirect(Prop, Key, ValuePtr, Ar);
		return;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ExecuteSavePlan (Step 4) — flat loop over a cached plan, optional delta gate
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::ExecuteSavePlan(const FArcTypePlan& Plan, const void* Data, FArcSaveArchive& Ar, const void* DefaultData)
{
	for (const FArcPlanEntry& Entry : Plan.Entries)
	{
		const void* ValuePtr = static_cast<const uint8*>(Data) + Entry.Offset;

		// Delta gate — inert when DefaultData == nullptr (the Save/SaveAll path). For the
		// Unsupported kind there is no Prop write anyway, so the gate is irrelevant there.
		if (DefaultData != nullptr && Entry.Kind != EArcPlanEntryKind::Unsupported)
		{
			const void* DefaultValuePtr = static_cast<const uint8*>(DefaultData) + Entry.Offset;
			if (Entry.Prop->Identical(ValuePtr, DefaultValuePtr, PPF_None))
			{
				continue;
			}
		}

		switch (Entry.Kind)
		{
		case EArcPlanEntryKind::Enum:
		{
			// Top-level enum uses the PRECOMPUTED EnumNameKey (do not rebuild the string).
			const FEnumProperty* EnumProp = CastFieldChecked<FEnumProperty>(Entry.Prop);
			FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
			const int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
			Ar.WriteProperty(Entry.Key, EnumValue);

			const UEnum* Enum = EnumProp->GetEnum();
			if (Enum)
			{
				const FString EnumValueName = Enum->GetNameStringByValue(EnumValue);
				Ar.WriteProperty(Entry.EnumNameKey, EnumValueName);
			}
			break;
		}
		case EArcPlanEntryKind::GenericStruct:
		{
			check(Entry.NestedPlan != nullptr);
			Ar.BeginStruct(Entry.Key);
			ExecuteSavePlan(
				*Entry.NestedPlan,
				ValuePtr,
				Ar,
				DefaultData != nullptr ? (static_cast<const uint8*>(DefaultData) + Entry.Offset) : nullptr);
			Ar.EndStruct();
			break;
		}
		case EArcPlanEntryKind::Array:
		{
			const FArrayProperty* ArrayProp = CastFieldChecked<FArrayProperty>(Entry.Prop);
			FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
			const int32 Num = ArrayHelper.Num();

			Ar.BeginArray(Entry.Key, Num);
			for (int32 i = 0; i < Num; ++i)
			{
				Ar.BeginArrayElement(i);
				// Element dispatch does NOT participate in the delta gate (no per-element default).
				ExecuteSaveValue(
					Entry.InnerKind, Entry.InnerProp, Entry.InnerNestedPlan,
					Entry.ElementKey, ArrayHelper.GetRawPtr(i), Ar);
				Ar.EndArrayElement();
			}
			Ar.EndArray();
			break;
		}
		case EArcPlanEntryKind::Map:
		{
			const FMapProperty* MapProp = CastFieldChecked<FMapProperty>(Entry.Prop);
			FScriptMapHelper MapHelper(MapProp, ValuePtr);
			const int32 Num = MapHelper.Num();

			Ar.BeginArray(Entry.Key, Num);
			int32 ElementIndex = 0;
			for (int32 SparseIndex = 0; SparseIndex < MapHelper.GetMaxIndex(); ++SparseIndex)
			{
				if (!MapHelper.IsValidIndex(SparseIndex))
				{
					continue;
				}

				Ar.BeginArrayElement(ElementIndex);

				const void* PairKeyPtr = MapHelper.GetKeyPtr(SparseIndex);
				const void* PairValuePtr = MapHelper.GetValuePtr(SparseIndex);

				// Key side → literal "k"; value side → "v" (== Entry.ElementKey).
				ExecuteSaveValue(
					Entry.MapKeyKind, Entry.MapKeyProp, /*NestedPlan=*/nullptr,
					FName("k"), PairKeyPtr, Ar);
				ExecuteSaveValue(
					Entry.InnerKind, Entry.InnerProp, Entry.InnerNestedPlan,
					Entry.ElementKey, PairValuePtr, Ar);

				Ar.EndArrayElement();
				++ElementIndex;
			}
			Ar.EndArray();
			break;
		}
		case EArcPlanEntryKind::Set:
		{
			const FSetProperty* SetProp = CastFieldChecked<FSetProperty>(Entry.Prop);
			FScriptSetHelper SetHelper(SetProp, ValuePtr);
			const int32 Num = SetHelper.Num();

			Ar.BeginArray(Entry.Key, Num);
			int32 ElementIndex = 0;
			for (int32 SparseIndex = 0; SparseIndex < SetHelper.GetMaxIndex(); ++SparseIndex)
			{
				if (!SetHelper.IsValidIndex(SparseIndex))
				{
					continue;
				}

				Ar.BeginArrayElement(ElementIndex);
				ExecuteSaveValue(
					Entry.InnerKind, Entry.InnerProp, Entry.InnerNestedPlan,
					Entry.ElementKey, SetHelper.GetElementPtr(SparseIndex), Ar);
				Ar.EndArrayElement();
				++ElementIndex;
			}
			Ar.EndArray();
			break;
		}
		case EArcPlanEntryKind::Unsupported:
		{
			// SaveValueDirect logs a warning and writes nothing — reproduce that, no archive output.
			UE_LOG(LogArcReflectionSerializer, Warning,
				TEXT("Skipping unsupported property type '%s' for property '%s'."),
				*Entry.Prop->GetClass()->GetName(), *Entry.Prop->GetName());
			break;
		}
		default:
			// All leaf scalars + known structs + RegisteredStruct share ExecuteSaveValue.
			ExecuteSaveValue(Entry.Kind, Entry.Prop, Entry.NestedPlan, Entry.Key, ValuePtr, Ar);
			break;
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Save
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::Save(const UStruct* Type, const void* Data, FArcSaveArchive& Ar)
{
	ExecuteSavePlan(GetOrBuildPlan(Type, /*bSaveGameOnly=*/true), Data, Ar, /*DefaultData=*/nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveProperty — extracts value pointer from container, then delegates
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::SaveProperty(FProperty* Prop, const void* ContainerData, FArcSaveArchive& Ar)
{
	const FName Key = Prop->GetFName();
	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerData);
	SaveValueDirect(Prop, Key, ValuePtr, Ar);
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveValueDirect — dispatch by FProperty subtype given a raw value pointer
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::SaveValueDirect(FProperty* Prop, FName Key, const void* ValuePtr, FArcSaveArchive& Ar)
{
	// ── Bool ────────────────────────────────────────────────────────────

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		Ar.WriteProperty(Key, BoolProp->GetPropertyValue(ValuePtr));
		return;
	}

	// ── Integer types ───────────────────────────────────────────────────

	if (CastField<FIntProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const int32*>(ValuePtr));
		return;
	}

	if (CastField<FInt64Property>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const int64*>(ValuePtr));
		return;
	}

	if (CastField<FByteProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const uint8*>(ValuePtr));
		return;
	}

	if (CastField<FUInt16Property>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const uint16*>(ValuePtr));
		return;
	}

	if (CastField<FUInt32Property>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const uint32*>(ValuePtr));
		return;
	}

	if (CastField<FUInt64Property>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const uint64*>(ValuePtr));
		return;
	}

	// ── Floating point ──────────────────────────────────────────────────

	if (CastField<FFloatProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const float*>(ValuePtr));
		return;
	}

	if (CastField<FDoubleProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const double*>(ValuePtr));
		return;
	}

	// ── String / Name / Text ────────────────────────────────────────────

	if (CastField<FStrProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const FString*>(ValuePtr));
		return;
	}

	if (CastField<FNameProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const FName*>(ValuePtr));
		return;
	}

	if (CastField<FTextProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const FText*>(ValuePtr));
		return;
	}

	// ── Enum ────────────────────────────────────────────────────────────

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		const int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		Ar.WriteProperty(Key, EnumValue);

		// Write the enum value name as a debug string alongside the numeric value
		const FName EnumNameKey(*FString::Printf(TEXT("%s_Name"), *Key.ToString()));
		const UEnum* Enum = EnumProp->GetEnum();
		if (Enum)
		{
			const FString EnumValueName = Enum->GetNameStringByValue(EnumValue);
			Ar.WriteProperty(EnumNameKey, EnumValueName);
		}
		return;
	}

	// ── Struct ──────────────────────────────────────────────────────────

	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		const UScriptStruct* Struct = StructProp->Struct;
		const FName StructFName = Struct->GetFName();

		// Known types — use typed WriteProperty overloads
		static const FName NAME_FGuid(TEXT("Guid"));
		static const FName NAME_GameplayTag(TEXT("GameplayTag"));
		static const FName NAME_GameplayTagContainer(TEXT("GameplayTagContainer"));

		if (StructFName == NAME_FGuid)
		{
			Ar.WriteProperty(Key, *static_cast<const FGuid*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_GameplayTag)
		{
			Ar.WriteProperty(Key, *static_cast<const FGameplayTag*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_GameplayTagContainer)
		{
			Ar.WriteProperty(Key, *static_cast<const FGameplayTagContainer*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Vector)
		{
			Ar.WriteProperty(Key, *static_cast<const FVector*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Rotator)
		{
			Ar.WriteProperty(Key, *static_cast<const FRotator*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Transform)
		{
			Ar.WriteProperty(Key, *static_cast<const FTransform*>(ValuePtr));
			return;
		}

		static const FName NAME_PrimaryAssetId(TEXT("PrimaryAssetId"));
		if (StructFName == NAME_PrimaryAssetId)
		{
			const FPrimaryAssetId& Id = *static_cast<const FPrimaryAssetId*>(ValuePtr);
			Ar.WriteProperty(Key, Id.ToString());
			return;
		}

		// Check registry for custom serializer before generic recursion
		const FArcPersistenceSerializerInfo* RegisteredInfo =
			FArcSerializerRegistry::Get().Find(Struct);
		if (RegisteredInfo)
		{
			Ar.BeginStruct(Key);
			RegisteredInfo->SaveFunc(ValuePtr, Ar);
			Ar.EndStruct();
			return;
		}

		// Generic struct — recurse
		Ar.BeginStruct(Key);
		Save(Struct, ValuePtr, Ar);
		Ar.EndStruct();
		return;
	}

	// ── Array ───────────────────────────────────────────────────────────

	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		const int32 Num = ArrayHelper.Num();

		Ar.BeginArray(Key, Num);
		for (int32 i = 0; i < Num; ++i)
		{
			Ar.BeginArrayElement(i);

			FProperty* InnerProp = ArrayProp->Inner;
			const void* ElementPtr = ArrayHelper.GetRawPtr(i);

			// Use a value key matching the array element context.
			// Inside an array element scope the key is typically "value" or the property name,
			// but the archive element scope handles naming — we just write with the same key.
			SaveValueDirect(InnerProp, Key, ElementPtr, Ar);

			Ar.EndArrayElement();
		}
		Ar.EndArray();
		return;
	}

	// ── Map ────────────────────────────────────────────────────────────

	if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
	{
		FScriptMapHelper MapHelper(MapProp, ValuePtr);
		const int32 Num = MapHelper.Num();

		Ar.BeginArray(Key, Num);
		int32 ElementIndex = 0;
		for (int32 SparseIndex = 0; SparseIndex < MapHelper.GetMaxIndex(); ++SparseIndex)
		{
			if (!MapHelper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			Ar.BeginArrayElement(ElementIndex);

			const void* PairKeyPtr = MapHelper.GetKeyPtr(SparseIndex);
			const void* PairValuePtr = MapHelper.GetValuePtr(SparseIndex);

			SaveValueDirect(MapProp->KeyProp, FName("k"), PairKeyPtr, Ar);
			SaveValueDirect(MapProp->ValueProp, FName("v"), PairValuePtr, Ar);

			Ar.EndArrayElement();
			++ElementIndex;
		}
		Ar.EndArray();
		return;
	}

	// ── Set ────────────────────────────────────────────────────────────

	if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
	{
		FScriptSetHelper SetHelper(SetProp, ValuePtr);
		const int32 Num = SetHelper.Num();

		Ar.BeginArray(Key, Num);
		int32 ElementIndex = 0;
		for (int32 SparseIndex = 0; SparseIndex < SetHelper.GetMaxIndex(); ++SparseIndex)
		{
			if (!SetHelper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			Ar.BeginArrayElement(ElementIndex);

			const void* ElemPtr = SetHelper.GetElementPtr(SparseIndex);
			SaveValueDirect(SetProp->ElementProp, Key, ElemPtr, Ar);

			Ar.EndArrayElement();
			++ElementIndex;
		}
		Ar.EndArray();
		return;
	}

	// ── Soft object reference ───────────────────────────────────────────

	if (CastField<FSoftObjectProperty>(Prop))
	{
		const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
		Ar.WriteProperty(Key, SoftPtr.ToString());
		return;
	}

	// ── Hard object reference ──────────────────────────────────────────

	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
	{
		const UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
		if (Obj)
		{
			UE_LOG(LogArcReflectionSerializer, Warning,
				TEXT("Serializing hard object reference for property '%s' (%s) — consider using TSoftObjectPtr for persistence."),
				*Prop->GetName(), *Obj->GetPathName());
			const FSoftObjectPath SoftPath(Obj);
			Ar.WriteProperty(Key, SoftPath.ToString());
		}
		else
		{
			Ar.WriteProperty(Key, FString());
		}
		return;
	}

	// ── Unsupported type ────────────────────────────────────────────────

	UE_LOG(LogArcReflectionSerializer, Warning,
		TEXT("Skipping unsupported property type '%s' for property '%s'."),
		*Prop->GetClass()->GetName(), *Prop->GetName());
}

// ─────────────────────────────────────────────────────────────────────────────
// ExecuteLoadValue (Step 5) — read one planned value given a raw value pointer
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::ExecuteLoadValue(
	EArcPlanEntryKind Kind,
	FProperty* Prop,
	const FArcTypePlan* NestedPlan,
	FName Key,
	void* ValuePtr,
	FArcLoadArchive& Ar)
{
	switch (Kind)
	{
	case EArcPlanEntryKind::Bool:
	{
		const FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(Prop);
		bool TempValue = BoolProp->GetPropertyValue(ValuePtr);
		if (Ar.ReadProperty(Key, TempValue))
		{
			BoolProp->SetPropertyValue(ValuePtr, TempValue);
		}
		return;
	}
	case EArcPlanEntryKind::Int32:
		Ar.ReadProperty(Key, *static_cast<int32*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Int64:
		Ar.ReadProperty(Key, *static_cast<int64*>(ValuePtr));
		return;
	case EArcPlanEntryKind::UInt8:
		Ar.ReadProperty(Key, *static_cast<uint8*>(ValuePtr));
		return;
	case EArcPlanEntryKind::UInt16:
		Ar.ReadProperty(Key, *static_cast<uint16*>(ValuePtr));
		return;
	case EArcPlanEntryKind::UInt32:
		Ar.ReadProperty(Key, *static_cast<uint32*>(ValuePtr));
		return;
	case EArcPlanEntryKind::UInt64:
		Ar.ReadProperty(Key, *static_cast<uint64*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Float:
		Ar.ReadProperty(Key, *static_cast<float*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Double:
		Ar.ReadProperty(Key, *static_cast<double*>(ValuePtr));
		return;
	case EArcPlanEntryKind::String:
		Ar.ReadProperty(Key, *static_cast<FString*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Name:
		Ar.ReadProperty(Key, *static_cast<FName*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Text:
		Ar.ReadProperty(Key, *static_cast<FText*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Enum:
	{
		// Match LoadValueDirect: read the numeric value via the underlying property, write it back
		// only on a successful read. The "<Key>_Name" debug string is write-only and never read.
		const FEnumProperty* EnumProp = CastFieldChecked<FEnumProperty>(Prop);
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		if (Ar.ReadProperty(Key, EnumValue))
		{
			UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumValue);
		}
		return;
	}
	case EArcPlanEntryKind::Guid:
		Ar.ReadProperty(Key, *static_cast<FGuid*>(ValuePtr));
		return;
	case EArcPlanEntryKind::GameplayTag:
		Ar.ReadProperty(Key, *static_cast<FGameplayTag*>(ValuePtr));
		return;
	case EArcPlanEntryKind::GameplayTagContainer:
		Ar.ReadProperty(Key, *static_cast<FGameplayTagContainer*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Vector:
		Ar.ReadProperty(Key, *static_cast<FVector*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Rotator:
		Ar.ReadProperty(Key, *static_cast<FRotator*>(ValuePtr));
		return;
	case EArcPlanEntryKind::Transform:
		Ar.ReadProperty(Key, *static_cast<FTransform*>(ValuePtr));
		return;
	case EArcPlanEntryKind::PrimaryAssetId:
	{
		FString AssetIdStr;
		if (Ar.ReadProperty(Key, AssetIdStr))
		{
			*static_cast<FPrimaryAssetId*>(ValuePtr) = FPrimaryAssetId(AssetIdStr);
		}
		return;
	}
	case EArcPlanEntryKind::SoftObject:
	{
		FString SoftPathStr;
		if (Ar.ReadProperty(Key, SoftPathStr))
		{
			FSoftObjectPtr& SoftPtr = *static_cast<FSoftObjectPtr*>(ValuePtr);
			SoftPtr = FSoftObjectPath(SoftPathStr);
		}
		return;
	}
	case EArcPlanEntryKind::HardObject:
	{
		const FObjectPropertyBase* ObjProp = CastFieldChecked<FObjectPropertyBase>(Prop);
		FString PathStr;
		if (Ar.ReadProperty(Key, PathStr))
		{
			if (PathStr.IsEmpty())
			{
				ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
			}
			else
			{
				const FSoftObjectPath SoftPath(PathStr);
				UObject* Loaded = SoftPath.TryLoad();
				if (Loaded)
				{
					ObjProp->SetObjectPropertyValue(ValuePtr, Loaded);
				}
				else
				{
					UE_LOG(LogArcReflectionSerializer, Warning,
						TEXT("Failed to resolve object path '%s' for property '%s'."),
						*PathStr, *Prop->GetName());
				}
			}
		}
		return;
	}
	case EArcPlanEntryKind::RegisteredStruct:
	{
		// Re-look-up the registry at execution time so the plan survives re-registration.
		const FStructProperty* StructProp = CastFieldChecked<FStructProperty>(Prop);
		const FArcPersistenceSerializerInfo* RegisteredInfo =
			FArcSerializerRegistry::Get().Find(StructProp->Struct);
		if (RegisteredInfo)
		{
			if (Ar.BeginStruct(Key))
			{
				RegisteredInfo->LoadFunc(ValuePtr, Ar);
				Ar.EndStruct();
			}
		}
		else
		{
			// Registration was removed since the plan was built — fall back to a generic
			// struct recurse so input parses (matches LoadValueDirect when no registered serializer).
			if (Ar.BeginStruct(Key))
			{
				Load(StructProp->Struct, ValuePtr, Ar);
				Ar.EndStruct();
			}
		}
		return;
	}
	case EArcPlanEntryKind::GenericStruct:
	{
		check(NestedPlan != nullptr);
		if (Ar.BeginStruct(Key))
		{
			ExecuteLoadPlan(*NestedPlan, ValuePtr, Ar);
			Ar.EndStruct();
		}
		return;
	}
	case EArcPlanEntryKind::Array:
	case EArcPlanEntryKind::Map:
	case EArcPlanEntryKind::Set:
	case EArcPlanEntryKind::Unsupported:
	default:
		// Container element is itself a container, or an unsupported type — fall back to the
		// legacy recursive reader. Byte-identical for the deep-nesting case (planned v1 behavior).
		LoadValueDirect(Prop, Key, ValuePtr, Ar);
		return;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ExecuteLoadPlan (Step 5) — flat loop over a cached plan, mirroring LoadValueDirect
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::ExecuteLoadPlan(const FArcTypePlan& Plan, void* Data, FArcLoadArchive& Ar)
{
	for (const FArcPlanEntry& Entry : Plan.Entries)
	{
		void* ValuePtr = static_cast<uint8*>(Data) + Entry.Offset;

		switch (Entry.Kind)
		{
		case EArcPlanEntryKind::Enum:
		{
			// Match LoadValueDirect: numeric read + write-back via the underlying property.
			// The precomputed EnumNameKey is write-only/debug and is NOT read back.
			const FEnumProperty* EnumProp = CastFieldChecked<FEnumProperty>(Entry.Prop);
			FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
			int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
			if (Ar.ReadProperty(Entry.Key, EnumValue))
			{
				UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumValue);
			}
			break;
		}
		case EArcPlanEntryKind::GenericStruct:
		{
			check(Entry.NestedPlan != nullptr);
			if (Ar.BeginStruct(Entry.Key))
			{
				ExecuteLoadPlan(*Entry.NestedPlan, ValuePtr, Ar);
				Ar.EndStruct();
			}
			break;
		}
		case EArcPlanEntryKind::Array:
		{
			const FArrayProperty* ArrayProp = CastFieldChecked<FArrayProperty>(Entry.Prop);
			int32 Num = 0;
			if (!Ar.BeginArray(Entry.Key, Num))
			{
				break;
			}

			FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
			ArrayHelper.Resize(Num);

			for (int32 i = 0; i < Num; ++i)
			{
				if (!Ar.BeginArrayElement(i))
				{
					break;
				}
				ExecuteLoadValue(
					Entry.InnerKind, Entry.InnerProp, Entry.InnerNestedPlan,
					Entry.ElementKey, ArrayHelper.GetRawPtr(i), Ar);
				Ar.EndArrayElement();
			}
			Ar.EndArray();
			break;
		}
		case EArcPlanEntryKind::Map:
		{
			const FMapProperty* MapProp = CastFieldChecked<FMapProperty>(Entry.Prop);
			int32 Num = 0;
			if (!Ar.BeginArray(Entry.Key, Num))
			{
				break;
			}

			FScriptMapHelper MapHelper(MapProp, ValuePtr);
			MapHelper.EmptyValues();

			for (int32 i = 0; i < Num; ++i)
			{
				if (!Ar.BeginArrayElement(i))
				{
					break;
				}

				const int32 NewIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
				void* PairKeyPtr = MapHelper.GetKeyPtr(NewIndex);
				void* PairValuePtr = MapHelper.GetValuePtr(NewIndex);

				// Key side → literal "k"; value side → "v" (== Entry.ElementKey).
				ExecuteLoadValue(
					Entry.MapKeyKind, Entry.MapKeyProp, /*NestedPlan=*/nullptr,
					FName("k"), PairKeyPtr, Ar);
				ExecuteLoadValue(
					Entry.InnerKind, Entry.InnerProp, Entry.InnerNestedPlan,
					Entry.ElementKey, PairValuePtr, Ar);

				Ar.EndArrayElement();
			}

			MapHelper.Rehash();
			Ar.EndArray();
			break;
		}
		case EArcPlanEntryKind::Set:
		{
			const FSetProperty* SetProp = CastFieldChecked<FSetProperty>(Entry.Prop);
			int32 Num = 0;
			if (!Ar.BeginArray(Entry.Key, Num))
			{
				break;
			}

			FScriptSetHelper SetHelper(SetProp, ValuePtr);
			SetHelper.EmptyElements();

			for (int32 i = 0; i < Num; ++i)
			{
				if (!Ar.BeginArrayElement(i))
				{
					break;
				}

				const int32 NewIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
				void* ElemPtr = SetHelper.GetElementPtr(NewIndex);

				ExecuteLoadValue(
					Entry.InnerKind, Entry.InnerProp, Entry.InnerNestedPlan,
					Entry.ElementKey, ElemPtr, Ar);

				Ar.EndArrayElement();
			}

			SetHelper.Rehash();
			Ar.EndArray();
			break;
		}
		case EArcPlanEntryKind::Unsupported:
		{
			// LoadValueDirect logs a warning and reads nothing — reproduce that, no archive read.
			UE_LOG(LogArcReflectionSerializer, Warning,
				TEXT("Skipping unsupported property type '%s' for property '%s'."),
				*Entry.Prop->GetClass()->GetName(), *Entry.Prop->GetName());
			break;
		}
		default:
			// All leaf scalars + known structs + RegisteredStruct share ExecuteLoadValue.
			ExecuteLoadValue(Entry.Kind, Entry.Prop, Entry.NestedPlan, Entry.Key, ValuePtr, Ar);
			break;
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Load
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::Load(const UStruct* Type, void* Data, FArcLoadArchive& Ar)
{
	ExecuteLoadPlan(GetOrBuildPlan(Type, /*bSaveGameOnly=*/true), Data, Ar);
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadProperty — extracts value pointer from container, then delegates
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::LoadProperty(FProperty* Prop, void* ContainerData, FArcLoadArchive& Ar)
{
	const FName Key = Prop->GetFName();
	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerData);
	LoadValueDirect(Prop, Key, ValuePtr, Ar);
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadValueDirect — dispatch by FProperty subtype given a raw value pointer
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::LoadValueDirect(FProperty* Prop, FName Key, void* ValuePtr, FArcLoadArchive& Ar)
{
	// ── Bool ────────────────────────────────────────────────────────────

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		bool TempValue = BoolProp->GetPropertyValue(ValuePtr);
		if (Ar.ReadProperty(Key, TempValue))
		{
			BoolProp->SetPropertyValue(ValuePtr, TempValue);
		}
		return;
	}

	// ── Integer types ───────────────────────────────────────────────────

	if (CastField<FIntProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<int32*>(ValuePtr));
		return;
	}

	if (CastField<FInt64Property>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<int64*>(ValuePtr));
		return;
	}

	if (CastField<FByteProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<uint8*>(ValuePtr));
		return;
	}

	if (CastField<FUInt16Property>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<uint16*>(ValuePtr));
		return;
	}

	if (CastField<FUInt32Property>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<uint32*>(ValuePtr));
		return;
	}

	if (CastField<FUInt64Property>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<uint64*>(ValuePtr));
		return;
	}

	// ── Floating point ──────────────────────────────────────────────────

	if (CastField<FFloatProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<float*>(ValuePtr));
		return;
	}

	if (CastField<FDoubleProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<double*>(ValuePtr));
		return;
	}

	// ── String / Name / Text ────────────────────────────────────────────

	if (CastField<FStrProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<FString*>(ValuePtr));
		return;
	}

	if (CastField<FNameProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<FName*>(ValuePtr));
		return;
	}

	if (CastField<FTextProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<FText*>(ValuePtr));
		return;
	}

	// ── Enum ────────────────────────────────────────────────────────────

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		if (Ar.ReadProperty(Key, EnumValue))
		{
			UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumValue);
		}
		// The _Name field is debug-only, we don't read it back
		return;
	}

	// ── Struct ──────────────────────────────────────────────────────────

	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		const UScriptStruct* Struct = StructProp->Struct;
		const FName StructFName = Struct->GetFName();

		// Known types — use typed ReadProperty overloads
		static const FName NAME_FGuid(TEXT("Guid"));
		static const FName NAME_GameplayTag(TEXT("GameplayTag"));
		static const FName NAME_GameplayTagContainer(TEXT("GameplayTagContainer"));

		if (StructFName == NAME_FGuid)
		{
			Ar.ReadProperty(Key, *static_cast<FGuid*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_GameplayTag)
		{
			Ar.ReadProperty(Key, *static_cast<FGameplayTag*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_GameplayTagContainer)
		{
			Ar.ReadProperty(Key, *static_cast<FGameplayTagContainer*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Vector)
		{
			Ar.ReadProperty(Key, *static_cast<FVector*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Rotator)
		{
			Ar.ReadProperty(Key, *static_cast<FRotator*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Transform)
		{
			Ar.ReadProperty(Key, *static_cast<FTransform*>(ValuePtr));
			return;
		}

		static const FName NAME_PrimaryAssetId(TEXT("PrimaryAssetId"));
		if (StructFName == NAME_PrimaryAssetId)
		{
			FString AssetIdStr;
			if (Ar.ReadProperty(Key, AssetIdStr))
			{
				*static_cast<FPrimaryAssetId*>(ValuePtr) = FPrimaryAssetId(AssetIdStr);
			}
			return;
		}

		// Check registry for custom serializer before generic recursion
		const FArcPersistenceSerializerInfo* RegisteredInfo =
			FArcSerializerRegistry::Get().Find(Struct);
		if (RegisteredInfo)
		{
			if (Ar.BeginStruct(Key))
			{
				RegisteredInfo->LoadFunc(ValuePtr, Ar);
				Ar.EndStruct();
			}
			return;
		}

		// Generic struct — recurse
		if (Ar.BeginStruct(Key))
		{
			Load(Struct, ValuePtr, Ar);
			Ar.EndStruct();
		}
		return;
	}

	// ── Array ───────────────────────────────────────────────────────────

	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		int32 Num = 0;
		if (!Ar.BeginArray(Key, Num))
		{
			return;
		}

		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		ArrayHelper.Resize(Num);

		for (int32 i = 0; i < Num; ++i)
		{
			if (!Ar.BeginArrayElement(i))
			{
				break;
			}

			FProperty* InnerProp = ArrayProp->Inner;
			void* ElementPtr = ArrayHelper.GetRawPtr(i);

			LoadValueDirect(InnerProp, Key, ElementPtr, Ar);

			Ar.EndArrayElement();
		}
		Ar.EndArray();
		return;
	}

	// ── Map ────────────────────────────────────────────────────────────

	if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
	{
		int32 Num = 0;
		if (!Ar.BeginArray(Key, Num))
		{
			return;
		}

		FScriptMapHelper MapHelper(MapProp, ValuePtr);
		MapHelper.EmptyValues();

		for (int32 i = 0; i < Num; ++i)
		{
			if (!Ar.BeginArrayElement(i))
			{
				break;
			}

			const int32 NewIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			void* PairKeyPtr = MapHelper.GetKeyPtr(NewIndex);
			void* PairValuePtr = MapHelper.GetValuePtr(NewIndex);

			LoadValueDirect(MapProp->KeyProp, FName("k"), PairKeyPtr, Ar);
			LoadValueDirect(MapProp->ValueProp, FName("v"), PairValuePtr, Ar);

			Ar.EndArrayElement();
		}

		MapHelper.Rehash();
		Ar.EndArray();
		return;
	}

	// ── Set ────────────────────────────────────────────────────────────

	if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
	{
		int32 Num = 0;
		if (!Ar.BeginArray(Key, Num))
		{
			return;
		}

		FScriptSetHelper SetHelper(SetProp, ValuePtr);
		SetHelper.EmptyElements();

		for (int32 i = 0; i < Num; ++i)
		{
			if (!Ar.BeginArrayElement(i))
			{
				break;
			}

			const int32 NewIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
			void* ElemPtr = SetHelper.GetElementPtr(NewIndex);

			LoadValueDirect(SetProp->ElementProp, Key, ElemPtr, Ar);

			Ar.EndArrayElement();
		}

		SetHelper.Rehash();
		Ar.EndArray();
		return;
	}

	// ── Soft object reference ───────────────────────────────────────────

	if (CastField<FSoftObjectProperty>(Prop))
	{
		FString SoftPathStr;
		if (Ar.ReadProperty(Key, SoftPathStr))
		{
			FSoftObjectPtr& SoftPtr = *static_cast<FSoftObjectPtr*>(ValuePtr);
			SoftPtr = FSoftObjectPath(SoftPathStr);
		}
		return;
	}

	// ── Hard object reference ──────────────────────────────────────────

	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
	{
		FString PathStr;
		if (Ar.ReadProperty(Key, PathStr))
		{
			if (PathStr.IsEmpty())
			{
				ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
			}
			else
			{
				const FSoftObjectPath SoftPath(PathStr);
				UObject* Loaded = SoftPath.TryLoad();
				if (Loaded)
				{
					ObjProp->SetObjectPropertyValue(ValuePtr, Loaded);
				}
				else
				{
					UE_LOG(LogArcReflectionSerializer, Warning,
						TEXT("Failed to resolve object path '%s' for property '%s'."),
						*PathStr, *Prop->GetName());
				}
			}
		}
		return;
	}

	// ── Unsupported type ────────────────────────────────────────────────

	UE_LOG(LogArcReflectionSerializer, Warning,
		TEXT("Skipping unsupported property type '%s' for property '%s'."),
		*Prop->GetClass()->GetName(), *Prop->GetName());
}

// ─────────────────────────────────────────────────────────────────────────────
// ComputeSchemaVersion
// ─────────────────────────────────────────────────────────────────────────────

uint32 FArcReflectionSerializer::ComputeSchemaVersion(const UStruct* Type)
{
	return GetOrBuildPlan(Type, /*bSaveGameOnly=*/true).SchemaVersion;
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveAll — serialize all properties (no SaveGame filter)
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::SaveAll(const UStruct* Type, const void* Data, FArcSaveArchive& Ar)
{
	ExecuteSavePlan(GetOrBuildPlan(Type, /*bSaveGameOnly=*/false), Data, Ar, /*DefaultData=*/nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadAll — deserialize all properties (no SaveGame filter)
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::LoadAll(const UStruct* Type, void* Data, FArcLoadArchive& Ar)
{
	ExecuteLoadPlan(GetOrBuildPlan(Type, /*bSaveGameOnly=*/false), Data, Ar);
}

// ─────────────────────────────────────────────────────────────────────────────
// ComputeSchemaVersionAll — hash all properties (no SaveGame filter)
// ─────────────────────────────────────────────────────────────────────────────

uint32 FArcReflectionSerializer::ComputeSchemaVersionAll(const UStruct* Type)
{
	return GetOrBuildPlan(Type, /*bSaveGameOnly=*/false).SchemaVersion;
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveDelta* — opt-in delta-vs-default entry points (Step 7)
//
// v1 scope: leaf-property delta + whole-container skip-if-identical.
// Per-element array delta is out of scope for v1 — it requires load-side index
// remapping and is deferred to v2.
//
// Cost note: Prop->Identical on a large TArray/TMap is O(N) per save call.
// This is acceptable because the delta path is strictly opt-in (not free) — do
// not use SaveDelta* on hot paths with large containers unless profiled.
//
// Wire into NOTHING. No existing call site calls these in v1. The three indirect
// consumers (FArcMassFragmentSerializer, FArcItemStoreSerializer, attribute case)
// continue calling Save/SaveAll unchanged.
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::SaveDelta(const UStruct* Type, const void* Data, FArcSaveArchive& Ar, const void* DefaultData)
{
	check(DefaultData != nullptr);
	ExecuteSavePlan(GetOrBuildPlan(Type, /*bSaveGameOnly=*/true), Data, Ar, DefaultData);
}

void FArcReflectionSerializer::SaveDeltaAll(const UStruct* Type, const void* Data, FArcSaveArchive& Ar, const void* DefaultData)
{
	check(DefaultData != nullptr);
	ExecuteSavePlan(GetOrBuildPlan(Type, /*bSaveGameOnly=*/false), Data, Ar, DefaultData);
}

void FArcReflectionSerializer::SaveDeltaFromCDO(const UClass* Class, const void* Data, FArcSaveArchive& Ar)
{
	const UObject* CDO = Class->GetDefaultObject();
	// UClass is-a UStruct — cast is safe.
	SaveDelta(Class, Data, Ar, CDO);
}

void FArcReflectionSerializer::SaveDeltaFromScriptStructDefault(const UScriptStruct* Struct, const void* Data, FArcSaveArchive& Ar)
{
	void* DefaultBuffer = FMemory::Malloc(Struct->GetStructureSize(), Struct->GetMinAlignment());
	Struct->InitializeStruct(DefaultBuffer);
	SaveDelta(Struct, Data, Ar, DefaultBuffer);
	Struct->DestroyStruct(DefaultBuffer);
	FMemory::Free(DefaultBuffer);
}
