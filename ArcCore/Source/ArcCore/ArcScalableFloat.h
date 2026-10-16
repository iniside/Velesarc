/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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

#include "Curves/RealCurve.h"
#include "Curves/RichCurve.h"
#include "DataRegistryId.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "Math/RandomStream.h"
#include "Misc/DataValidation.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/ObjectMacros.h"

#include "ArcScalableFloat.generated.h"

class AActor;
struct FArcItemData;
struct FArcItemDefinitionData;
struct FArcScalableFloat;

struct FArcScalableFloatContext
{
	AActor* Owner = nullptr;
	AActor* Avatar = nullptr;
	const FArcItemData* ItemData = nullptr;
	const FArcItemDefinitionData* ItemDefinition = nullptr;
	/** Optional seeded stream for range rolls. Ignored by curve/None sources. */
	const FRandomStream* RandomStream = nullptr;
};

/**
 * Selects which curve source backs the scalable float value lookup.
 * Driven by an explicit enum (rather than inferring from non-empty fields) so the
 * editor UX is a simple combo + conditional widgets and there is no source-precedence
 * ambiguity.
 */
UENUM()
enum class EArcScalableFloatSource : uint8
{
	None,              // pure Value (IsStatic)
	InlineCurve,       // Value * InlineCurve.Eval(Level)
	CurveTableRow,     // Value * Curve.GetCurve()->Eval(Level)
	DataRegistryRow,   // Value * DataRegistrySubsystem::EvaluateCachedCurve(Level)
	InlineRange,       // FMath::FRandRange(MinValue, MaxValue) — rolled fresh per evaluation
	DataTableRange,    // roll between DataTable row's Min/Max (row struct = FArcScalableFloatRange)
	DataRegistryRange, // roll between DataRegistry-resolved FArcScalableFloatRange Min/Max
};

/** DataTable row struct for Min/Max range sources. Use as the RowType on DataTable assets
 *  backing DataTableRange FArcScalableFloat fields. */
USTRUCT(BlueprintType)
struct FArcScalableFloatRange : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	float Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	float Max = 0.f;
};

USTRUCT(BlueprintType)
struct FArcScalableFloatValueScaling
{
	GENERATED_BODY()

	virtual ~FArcScalableFloatValueScaling() = default;

	ARCCORE_API virtual float CalculateValue(const FArcScalableFloat& InValue, int32 Level, const FArcScalableFloatContext& Context) const;
};

/**
 * Generic numerical value in the form Value * Curve[Level].
 *
 * Standalone replacement for engine FScalableFloat (GameplayAbilities plugin). Keeps
 * the same persistent field names (Value, Curve, RegistryType, CustomScaling) so
 * existing assets migrate by-name. Three curve sources are supported through the
 * Source enum: inline FRichCurve, CurveTable row, or DataRegistry row.
 */
USTRUCT(BlueprintType)
struct FArcScalableFloat
{
	GENERATED_USTRUCT_BODY()

public:
	/** Raw value, multiplied by the curve lookup at Level (if any source is set). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	float Value;

	/** Which curve source provides the scaling factor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	EArcScalableFloatSource Source;

	/** Curve definition stored inline (no separate CurveTable asset needed). Used when Source==InlineCurve.
	 *  Not BlueprintReadWrite: FRichCurve isn't blueprint-exposable; matches engine FRuntimeFloatCurve.
	 *  Visibility/edit gating is handled by FArcScalableFloatDetails — no EditCondition on the UPROPERTY
	 *  so default editors and API writes work unconditionally. */
	UPROPERTY(EditAnywhere, Category = ScalableFloat)
	FRichCurve InlineCurve;

	/** Curve table row reference. Used when Source==CurveTableRow. Also used to carry the DataRegistry
	 *  row name (Curve.RowName) when Source==DataRegistryRow — matches engine FScalableFloat layout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	FCurveTableRowHandle Curve;

	/** Name of Data Registry containing curve to use. If set the RowName inside Curve is used as the item name. Used when Source==DataRegistryRow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	FDataRegistryType RegistryType;

	/** Optional custom scaling policy that overrides Value*Curve. Receives Level + context. */
	UPROPERTY(EditAnywhere, Category = ScalableFloat,
		meta = (BaseStruct = "/Script/ArcCore.ArcScalableFloatValueScaling", ExcludeBaseStruct))
	FInstancedStruct CustomScaling;

	/** Lower bound for InlineRange rolls. Ignored for other sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	float MinValue = 0.f;

	/** Upper bound for InlineRange rolls. Ignored for other sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	float MaxValue = 0.f;

	/** DataTable asset whose row provides Min/Max for DataTableRange. Row struct must be FArcScalableFloatRange. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	FDataTableRowHandle DataTable;

	/** Row name used by DataTableRange and DataRegistryRange lookups. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScalableFloat)
	FName RangeRowName;

	FArcScalableFloat()
		: Value(0.f)
		, Source(EArcScalableFloatSource::None)
		, MinValue(0.f)
		, MaxValue(0.f)
		, LocalCachedCurveID(INDEX_NONE)
		, CachedCurve(nullptr)
		, CachedRangeMin(0.f)
		, CachedRangeMax(0.f)
		, bRangeCached(false)
	{
	}

	FArcScalableFloat(float InInitialValue)
		: Value(InInitialValue)
		, Source(EArcScalableFloatSource::None)
		, MinValue(0.f)
		, MaxValue(0.f)
		, LocalCachedCurveID(INDEX_NONE)
		, CachedCurve(nullptr)
		, CachedRangeMin(0.f)
		, CachedRangeMax(0.f)
		, bRangeCached(false)
	{
	}

	~FArcScalableFloat() = default;

	// ---- Engine-parity evaluation API ----

	/** Returns the scaled value at a given level. */
	ARCCORE_API float GetValueAtLevel(float Level, const FString* ContextString = nullptr) const;

	/** Integer-level overload — useful in Mass hot paths (ability level, item stack size). */
	ARCCORE_API float GetValueAtLevel(int32 Level, const FString* ContextString = nullptr) const;

	/** Returns the scaled value at level 0. */
	ARCCORE_API float GetValue(const FString* ContextString = nullptr) const;

	/** Treats the scalable float as a bool (> 0). */
	ARCCORE_API bool AsBool(float Level = 0.f, const FString* ContextString = nullptr) const;

	/** Returns the value at the given level as an int32. */
	ARCCORE_API int32 AsInteger(float Level = 0.f, const FString* ContextString = nullptr) const;

	/** Gets the value and possible curve at a given level. Returns false if no value could be found. */
	ARCCORE_API bool EvaluateCurveAtLevel(float& OutValue, const FRealCurve*& OutCurve, float Level, const FString& ContextString, bool bWarnIfInvalid = true) const;

	/** True if there is no curve lookup (no source selected). */
	bool IsStatic() const { return Source == EArcScalableFloatSource::None; }

	/** Sets raw value and clears any curve source. */
	ARCCORE_API void SetValue(float NewValue);

	/** Overrides raw value and curve table reference (sets Source to CurveTableRow). */
	ARCCORE_API void SetScalingValue(float InCoefficient, FName InRowName, UCurveTable* InTable);

	/** Returns static value, only safe when no curve source is set. */
	float GetValueChecked() const
	{
		check(IsStatic());
		return Value;
	}

	/** True when Source is one of the three range sources. */
	bool IsRangeSource() const
	{
		return Source == EArcScalableFloatSource::InlineRange
			|| Source == EArcScalableFloatSource::DataTableRange
			|| Source == EArcScalableFloatSource::DataRegistryRange;
	}

	/** Multiplier applied to resolved range [Min,Max]. Value==0 is treated as 1 so legacy
	 *  range assets — which defaulted Value to 0 — keep rolling their full, unscaled range. */
	float GetRangeMultiplier() const { return Value == 0.f ? 1.f : Value; }

	/** Resolves the raw [Min,Max] of a range source. For non-range sources returns false
	 *  and sets OutMin==OutMax==GetValueAtLevel(0). Range values are level- and
	 *  CustomScaling-agnostic by design, so no Level param. */
	ARCCORE_API bool GetValueRange(float& OutMin, float& OutMax, const FString* ContextString = nullptr) const;

	/** Convenience: lower/upper/midpoint of the resolved range (or the static value). */
	ARCCORE_API float GetRangeMin(const FString* ContextString = nullptr) const;
	ARCCORE_API float GetRangeMax(const FString* ContextString = nullptr) const;
	ARCCORE_API float GetRangeMid(const FString* ContextString = nullptr) const;

	/** Human readable string for logging / property previews. */
	ARCCORE_API FString ToSimpleString() const;

	/** True if the configured curve source is valid (resolves to a real value at level 0). */
	ARCCORE_API bool IsValid() const;

	// ---- Context-aware evaluation (routes through CustomScaling if present) ----

	ARCCORE_API float GetScaledValue(int32 Level, const FArcScalableFloatContext& Context = FArcScalableFloatContext()) const;
	ARCCORE_API float GetScaledValue(float Level, const FArcScalableFloatContext& Context = FArcScalableFloatContext()) const;

	// ---- Serialization ----

	/**
	 * Upgrades a float / int8 / int16 / int32 / bool property into an FArcScalableFloat.
	 * Also migrates from the legacy engine FScalableFloat struct stored in older assets
	 * (StructName == "ScalableFloat"): copies Value/Curve/RegistryType and sets Source
	 * based on which legacy field was populated.
	 */
	ARCCORE_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
	ARCCORE_API void PostSerialize(const FArchive& Ar);

#if WITH_EDITOR
	ARCCORE_API EDataValidationResult IsDataValid(class FDataValidationContext& Context, const FString& PathName) const;
#endif

	ARCCORE_API bool operator==(const FArcScalableFloat& Other) const;
	ARCCORE_API bool operator!=(const FArcScalableFloat& Other) const;

	/** Inlined to dodge a cross-module dllexport quirk on the assignment operator symbol. */
	FArcScalableFloat& operator=(const FArcScalableFloat& Src)
	{
		if (this == &Src)
		{
			return *this;
		}
		Value = Src.Value;
		Source = Src.Source;
		InlineCurve = Src.InlineCurve;
		Curve = Src.Curve;
		RegistryType = Src.RegistryType;
		CustomScaling = Src.CustomScaling;
		MinValue = Src.MinValue;
		MaxValue = Src.MaxValue;
		DataTable = Src.DataTable;
		RangeRowName = Src.RangeRowName;
		ClearCachedCurve();
		return *this;
	}

private:
	/** Value of UCurveTable::GetGlobalCachedCurveID when CachedCurve was last refreshed. */
	mutable int32 LocalCachedCurveID;

	/** Cached direct pointer to the RealCurve resolved from CurveTable / DataRegistry source. */
	mutable const FRealCurve* CachedCurve;

	/** Cached Min resolved from DataTableRange row. Not used for InlineRange (no asset fetch). */
	mutable float CachedRangeMin = 0.f;

	/** Cached Max resolved from DataTableRange row. Not used for InlineRange (no asset fetch). */
	mutable float CachedRangeMax = 0.f;

	/** True when CachedRangeMin/Max are valid and the DataTableRange row has been fetched. */
	mutable bool bRangeCached = false;

	void ClearCachedCurve() const
	{
		LocalCachedCurveID = INDEX_NONE;
		CachedCurve = nullptr;
		bRangeCached = false;
	}

	/** Resolves Min/Max for range sources. Returns false if the source is misconfigured. */
	bool ResolveRange(float& OutMin, float& OutMax, const FString& ContextString, bool bWarnIfInvalid) const;

	/** Rolls a value in [Min,Max] using Stream (or FMath::FRandRange when null). */
	float RollRange(const FRandomStream* Stream, const FString& ContextString, bool bWarnIfInvalid) const;
};

template <>
struct TStructOpsTypeTraits<FArcScalableFloat>
	: public TStructOpsTypeTraitsBase2<FArcScalableFloat>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
		WithPostSerialize = true,
	};
};
