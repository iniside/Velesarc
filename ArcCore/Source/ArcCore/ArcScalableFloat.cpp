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

#include "ArcScalableFloat.h"

#include "DataRegistrySubsystem.h"
#include "Serialization/StructuredArchive.h"
#include "UObject/PropertyTag.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcScalableFloat)

DEFINE_LOG_CATEGORY_STATIC(LogArcScalableFloat, Log, All);

#define LOCTEXT_NAMESPACE "ArcScalableFloat"

float FArcScalableFloatValueScaling::CalculateValue(const FArcScalableFloat& InValue, int32 Level, const FArcScalableFloatContext& Context) const
{
	return InValue.GetValueAtLevel(Level);
}

bool FArcScalableFloat::ResolveRange(float& OutMin, float& OutMax, const FString& ContextString, bool bWarnIfInvalid) const
{
	switch (Source)
	{
	case EArcScalableFloatSource::InlineRange:
	{
		// Inline fields need no asset fetch and are never cached.
		OutMin = MinValue;
		OutMax = MaxValue;
		return true;
	}

	case EArcScalableFloatSource::DataTableRange:
	{
		if (bRangeCached)
		{
			OutMin = CachedRangeMin;
			OutMax = CachedRangeMax;
			return true;
		}

		if (!DataTable.DataTable)
		{
			if (bWarnIfInvalid)
			{
				UE_LOGF(LogArcScalableFloat, Warning,"FArcScalableFloat DataTableRange: no DataTable set (%ls).", *ContextString);
			}
			OutMin = OutMax = 0.f;
			return false;
		}

		if (DataTable.RowName.IsNone())
		{
			if (bWarnIfInvalid)
			{
				UE_LOGF(LogArcScalableFloat, Warning,"FArcScalableFloat DataTableRange: RowName is None in DataTable %ls (%ls).", *DataTable.DataTable->GetPathName(), *ContextString);
			}
			OutMin = OutMax = 0.f;
			return false;
		}

		// Verify the table carries the expected row struct before fetching.
		if (DataTable.DataTable->GetRowStruct() != FArcScalableFloatRange::StaticStruct())
		{
			if (bWarnIfInvalid)
			{
				UE_LOGF(LogArcScalableFloat, Warning,"FArcScalableFloat DataTableRange: DataTable %ls row struct is %ls, expected FArcScalableFloatRange (%ls).",
					*DataTable.DataTable->GetPathName(),
					*DataTable.DataTable->GetRowStruct()->GetName(),
					*ContextString);
			}
			OutMin = OutMax = 0.f;
			return false;
		}

		const FArcScalableFloatRange* Row = DataTable.GetRow<FArcScalableFloatRange>(ContextString);
		if (!Row)
		{
			if (bWarnIfInvalid)
			{
				UE_LOGF(LogArcScalableFloat, Warning,"FArcScalableFloat DataTableRange: row '%ls' not found in DataTable %ls (%ls).",
					*DataTable.RowName.ToString(), *DataTable.DataTable->GetPathName(), *ContextString);
			}
			OutMin = OutMax = 0.f;
			return false;
		}

		CachedRangeMin = Row->Min;
		CachedRangeMax = Row->Max;
		bRangeCached = true;
		OutMin = CachedRangeMin;
		OutMax = CachedRangeMax;
		return true;
	}

	case EArcScalableFloatSource::DataRegistryRange:
	{
		// DataRegistry items can be evicted from cache at any time; do not float-cache.
		if (!RegistryType.IsValid())
		{
			if (bWarnIfInvalid)
			{
				UE_LOGF(LogArcScalableFloat, Warning,"FArcScalableFloat DataRegistryRange: RegistryType is invalid (%ls).", *ContextString);
			}
			OutMin = OutMax = 0.f;
			return false;
		}

		if (RangeRowName.IsNone())
		{
			if (bWarnIfInvalid)
			{
				UE_LOGF(LogArcScalableFloat, Warning,"FArcScalableFloat DataRegistryRange: RangeRowName is None (%ls).", *ContextString);
			}
			OutMin = OutMax = 0.f;
			return false;
		}

		if (IsEngineExitRequested())
		{
			OutMin = OutMax = 0.f;
			return false;
		}

		UDataRegistrySubsystem* SubSystem = UDataRegistrySubsystem::Get();
		if (!SubSystem)
		{
			OutMin = OutMax = 0.f;
			return false;
		}

		const FArcScalableFloatRange* RangeItem = SubSystem->GetCachedItem<FArcScalableFloatRange>(FDataRegistryId(RegistryType, RangeRowName));
		if (!RangeItem)
		{
			if (bWarnIfInvalid)
			{
				UE_LOGF(LogArcScalableFloat, Warning,"FArcScalableFloat DataRegistryRange: item '%ls:%ls' not found in DataRegistry (%ls).",
					*RegistryType.ToString(), *RangeRowName.ToString(), *ContextString);
			}
			OutMin = OutMax = 0.f;
			return false;
		}

		OutMin = RangeItem->Min;
		OutMax = RangeItem->Max;
		return true;
	}

	default:
		OutMin = OutMax = 0.f;
		return false;
	}
}

float FArcScalableFloat::RollRange(const FRandomStream* Stream, const FString& ContextString, bool bWarnIfInvalid) const
{
	float RangeMin = 0.f;
	float RangeMax = 0.f;
	if (!ResolveRange(RangeMin, RangeMax, ContextString, bWarnIfInvalid))
	{
		return 0.f;
	}

	const float Roll = Stream
		? Stream->FRandRange(RangeMin, RangeMax)
		: FMath::FRandRange(RangeMin, RangeMax);

	return GetRangeMultiplier() * Roll;
}

bool FArcScalableFloat::GetValueRange(float& OutMin, float& OutMax, const FString* ContextString) const
{
	const FString Ctx = ContextString ? *ContextString : FString();
	if (IsRangeSource())
	{
		if (!ResolveRange(OutMin, OutMax, Ctx, /*bWarnIfInvalid*/ true))
		{
			return false;
		}
		const float Mult = GetRangeMultiplier();
		OutMin *= Mult;
		OutMax *= Mult;
		return true;
	}
	OutMin = OutMax = GetValueAtLevel(0.f, ContextString);
	return false;
}

float FArcScalableFloat::GetRangeMin(const FString* ContextString) const
{
	float OutMin = 0.f;
	float OutMax = 0.f;
	GetValueRange(OutMin, OutMax, ContextString);
	return OutMin;
}

float FArcScalableFloat::GetRangeMax(const FString* ContextString) const
{
	float OutMin = 0.f;
	float OutMax = 0.f;
	GetValueRange(OutMin, OutMax, ContextString);
	return OutMax;
}

float FArcScalableFloat::GetRangeMid(const FString* ContextString) const
{
	float OutMin = 0.f;
	float OutMax = 0.f;
	GetValueRange(OutMin, OutMax, ContextString);
	return (OutMin + OutMax) * 0.5f;
}

bool FArcScalableFloat::EvaluateCurveAtLevel(float& OutValue, const FRealCurve*& OutCurve, float Level, const FString& ContextString, bool bWarnIfInvalid) const
{
	switch (Source)
	{
	case EArcScalableFloatSource::None:
	{
		OutCurve = nullptr;
		OutValue = Value;
		return true;
	}

	case EArcScalableFloatSource::InlineCurve:
	{
		// FRichCurve doubles as FRealCurve via inheritance.
		OutCurve = &InlineCurve;
		OutValue = Value * InlineCurve.Eval(Level);
		return true;
	}

	case EArcScalableFloatSource::InlineRange:
	case EArcScalableFloatSource::DataTableRange:
	case EArcScalableFloatSource::DataRegistryRange:
	{
		// Range sources produce a random roll; no curve is involved.
		OutCurve = nullptr;
		OutValue = RollRange(nullptr, ContextString, bWarnIfInvalid);
		return true;
	}

	case EArcScalableFloatSource::CurveTableRow:
	{
		if (Curve.RowName.IsNone())
		{
			if (bWarnIfInvalid && Curve.CurveTable)
			{
				UE_LOGF(LogArcScalableFloat, Warning,"FArcScalableFloat has no row for CurveTable %ls (%ls).", *Curve.CurveTable->GetPathName(), *ContextString);
			}
			OutCurve = nullptr;
			OutValue = Value;
			return true;
		}

		// Invalidate cache if the global curve id changed (e.g. hot-reload of curve table data).
		const int32 GlobalCachedCurveID = UCurveTable::GetGlobalCachedCurveID();
		if (LocalCachedCurveID != GlobalCachedCurveID)
		{
			ClearCachedCurve();
		}

		OutCurve = CachedCurve;
		if (OutCurve == nullptr && Curve.CurveTable)
		{
			CachedCurve = OutCurve = Curve.GetCurve(ContextString, bWarnIfInvalid);
			LocalCachedCurveID = GlobalCachedCurveID;
		}

		if (OutCurve != nullptr)
		{
			OutValue = Value * OutCurve->Eval(Level);
			return true;
		}

		// Curve table missing or row not found; fall back to raw Value.
		OutValue = Value;
		return false;
	}

	case EArcScalableFloatSource::DataRegistryRow:
	{
		if (Curve.RowName.IsNone())
		{
			if (bWarnIfInvalid)
			{
				UE_LOGF(LogArcScalableFloat, Warning,"FArcScalableFloat has no row for DataRegistry %ls (%ls).", *RegistryType.ToString(), *ContextString);
			}
			OutCurve = nullptr;
			OutValue = Value;
			return true;
		}

		const int32 GlobalCachedCurveID = UCurveTable::GetGlobalCachedCurveID();
		if (LocalCachedCurveID != GlobalCachedCurveID)
		{
			ClearCachedCurve();
		}

		OutCurve = CachedCurve;
		if (OutCurve == nullptr && RegistryType.IsValid() && !IsEngineExitRequested())
		{
			UDataRegistrySubsystem* SubSystem = UDataRegistrySubsystem::Get();
			if (ensure(SubSystem))
			{
				float OutTempValue = 0.f;
				const FDataRegistryCacheGetResult GetResult = SubSystem->EvaluateCachedCurve(OutTempValue, OutCurve, FDataRegistryId(RegistryType, Curve.RowName), Level);

				if (GetResult)
				{
					if (GetResult.IsPersistent() && GetResult.GetVersionSource() == EDataRegistryCacheVersionSource::CurveTable)
					{
						CachedCurve = OutCurve;
						LocalCachedCurveID = GetResult.GetCacheVersion();
					}

					OutValue = Value * OutTempValue;
					return true;
				}
			}
		}

		if (OutCurve != nullptr)
		{
			OutValue = Value * OutCurve->Eval(Level);
			return true;
		}

		if (bWarnIfInvalid)
		{
			UE_LOGF(LogArcScalableFloat, Verbose,"FArcScalableFloat could not find curve for DataRegistryId %ls:%ls (%ls).", *RegistryType.ToString(), *Curve.RowName.ToString(), *ContextString);
		}

		OutValue = Value;
		return false;
	}
	}

	OutCurve = nullptr;
	OutValue = Value;
	return true;
}

float FArcScalableFloat::GetValueAtLevel(float Level, const FString* ContextString) const
{
	static const FString DefaultContextString = TEXT("FArcScalableFloat::GetValueAtLevel");
	float OutFloat = 0.f;
	const FRealCurve* FoundCurve = nullptr;
	EvaluateCurveAtLevel(OutFloat, FoundCurve, Level, ContextString != nullptr ? *ContextString : DefaultContextString, ContextString != nullptr);
	return OutFloat;
}

float FArcScalableFloat::GetValueAtLevel(int32 Level, const FString* ContextString) const
{
	return GetValueAtLevel(static_cast<float>(Level), ContextString);
}

float FArcScalableFloat::GetValue(const FString* ContextString) const
{
	return GetValueAtLevel(0.f, ContextString);
}

bool FArcScalableFloat::AsBool(float Level, const FString* ContextString) const
{
	return GetValueAtLevel(Level, ContextString) > 0.f;
}

int32 FArcScalableFloat::AsInteger(float Level, const FString* ContextString) const
{
	return static_cast<int32>(GetValueAtLevel(Level, ContextString));
}

void FArcScalableFloat::SetValue(float NewValue)
{
	Value = NewValue;
	Source = EArcScalableFloatSource::None;
	Curve.CurveTable = nullptr;
	Curve.RowName = NAME_None;
	MinValue = 0.f;
	MaxValue = 0.f;
	DataTable.DataTable = nullptr;
	DataTable.RowName = NAME_None;
	RangeRowName = NAME_None;
	ClearCachedCurve();
}

void FArcScalableFloat::SetScalingValue(float InCoefficient, FName InRowName, UCurveTable* InTable)
{
	Value = InCoefficient;
	Curve.RowName = InRowName;
	Curve.CurveTable = InTable;
	Source = EArcScalableFloatSource::CurveTableRow;
	MinValue = 0.f;
	MaxValue = 0.f;
	DataTable.DataTable = nullptr;
	DataTable.RowName = NAME_None;
	RangeRowName = NAME_None;
	ClearCachedCurve();
}

FString FArcScalableFloat::ToSimpleString() const
{
	switch (Source)
	{
	case EArcScalableFloatSource::CurveTableRow:
		if (Curve.CurveTable)
		{
			return FString::Printf(TEXT("%.2f - %s@%s"), Value, *Curve.RowName.ToString(), *Curve.CurveTable->GetName());
		}
		return FString::Printf(TEXT("%.2f - %s"), Value, *Curve.RowName.ToString());

	case EArcScalableFloatSource::DataRegistryRow:
		return FString::Printf(TEXT("%.2f - %s:%s"), Value, *RegistryType.ToString(), *Curve.RowName.ToString());

	case EArcScalableFloatSource::InlineCurve:
		return FString::Printf(TEXT("%.2f - inline curve (%d keys)"), Value, InlineCurve.GetNumKeys());

	case EArcScalableFloatSource::InlineRange:
		return FString::Printf(TEXT("range [%.2f, %.2f] inline"), MinValue, MaxValue);

	case EArcScalableFloatSource::DataTableRange:
	{
		const FString TableName = DataTable.DataTable ? DataTable.DataTable->GetName() : TEXT("<none>");
		float Min = 0.f;
		float Max = 0.f;
		if (ResolveRange(Min, Max, TEXT("FArcScalableFloat::ToSimpleString"), /*bWarnIfInvalid*/ false))
		{
			return FString::Printf(TEXT("range [%.2f, %.2f] %s@%s"), Min, Max, *DataTable.RowName.ToString(), *TableName);
		}
		return FString::Printf(TEXT("range %s@%s (unresolved)"), *DataTable.RowName.ToString(), *TableName);
	}

	case EArcScalableFloatSource::DataRegistryRange:
	{
		float Min = 0.f;
		float Max = 0.f;
		if (ResolveRange(Min, Max, TEXT("FArcScalableFloat::ToSimpleString"), /*bWarnIfInvalid*/ false))
		{
			return FString::Printf(TEXT("range [%.2f, %.2f] %s:%s"), Min, Max, *RegistryType.ToString(), *RangeRowName.ToString());
		}
		return FString::Printf(TEXT("range %s:%s (unresolved)"), *RegistryType.ToString(), *RangeRowName.ToString());
	}

	case EArcScalableFloatSource::None:
	default:
		return FString::Printf(TEXT("%.2f"), Value);
	}
}

bool FArcScalableFloat::IsValid() const
{
	static const FString DefaultContextString = TEXT("FArcScalableFloat::IsValid");

	// Range sources: valid if resolution succeeds and Max >= Min (a zero-width range is allowed).
	if (Source == EArcScalableFloatSource::InlineRange
		|| Source == EArcScalableFloatSource::DataTableRange
		|| Source == EArcScalableFloatSource::DataRegistryRange)
	{
		float RangeMin = 0.f;
		float RangeMax = 0.f;
		if (!ResolveRange(RangeMin, RangeMax, DefaultContextString, false))
		{
			return false;
		}
		return RangeMax >= RangeMin;
	}

	float OutFloat = 0.f;
	const FRealCurve* FoundCurve = nullptr;
	return EvaluateCurveAtLevel(OutFloat, FoundCurve, 0.f, DefaultContextString, false);
}

float FArcScalableFloat::GetScaledValue(int32 Level, const FArcScalableFloatContext& Context) const
{
	// Range sources bypass curve evaluation and CustomScaling — they produce a random roll.
	if (Source == EArcScalableFloatSource::InlineRange
		|| Source == EArcScalableFloatSource::DataTableRange
		|| Source == EArcScalableFloatSource::DataRegistryRange)
	{
		static const FString DefaultContextString = TEXT("FArcScalableFloat::GetScaledValue");
		return RollRange(Context.RandomStream, DefaultContextString, true);
	}

	if (const FArcScalableFloatValueScaling* Scaling = CustomScaling.GetPtr<FArcScalableFloatValueScaling>())
	{
		return Scaling->CalculateValue(*this, Level, Context);
	}
	return GetValueAtLevel(Level);
}

float FArcScalableFloat::GetScaledValue(float Level, const FArcScalableFloatContext& Context) const
{
	// Range sources bypass curve evaluation and CustomScaling — they produce a random roll.
	if (Source == EArcScalableFloatSource::InlineRange
		|| Source == EArcScalableFloatSource::DataTableRange
		|| Source == EArcScalableFloatSource::DataRegistryRange)
	{
		static const FString DefaultContextString = TEXT("FArcScalableFloat::GetScaledValue");
		return RollRange(Context.RandomStream, DefaultContextString, true);
	}

	if (const FArcScalableFloatValueScaling* Scaling = CustomScaling.GetPtr<FArcScalableFloatValueScaling>())
	{
		return Scaling->CalculateValue(*this, static_cast<int32>(Level), Context);
	}
	return GetValueAtLevel(Level);
}

bool FArcScalableFloat::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if (Tag.Type == NAME_FloatProperty)
	{
		float OldValue = 0.f;
		Slot << OldValue;
		*this = FArcScalableFloat(OldValue);
		return true;
	}
	if (Tag.Type == NAME_IntProperty)
	{
		int32 OldValue = 0;
		Slot << OldValue;
		*this = FArcScalableFloat(static_cast<float>(OldValue));
		return true;
	}
	if (Tag.Type == NAME_Int8Property)
	{
		int8 OldValue = 0;
		Slot << OldValue;
		*this = FArcScalableFloat(static_cast<float>(OldValue));
		return true;
	}
	if (Tag.Type == NAME_Int16Property)
	{
		int16 OldValue = 0;
		Slot << OldValue;
		*this = FArcScalableFloat(static_cast<float>(OldValue));
		return true;
	}
	if (Tag.Type == NAME_BoolProperty)
	{
		*this = FArcScalableFloat(Tag.BoolVal ? 1.f : 0.f);
		return true;
	}

	return false;
}

void FArcScalableFloat::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		// Heuristic: assets saved before Source existed had Source default to None even when
		// a curve source was actually populated. Promote based on which legacy field is set.
		if (Source == EArcScalableFloatSource::None)
		{
			if (!Curve.RowName.IsNone() && Curve.CurveTable)
			{
				Source = EArcScalableFloatSource::CurveTableRow;
			}
			else if (!Curve.RowName.IsNone() && RegistryType.IsValid())
			{
				Source = EArcScalableFloatSource::DataRegistryRow;
			}
			else if (InlineCurve.GetNumKeys() > 0)
			{
				Source = EArcScalableFloatSource::InlineCurve;
			}
		}

		ClearCachedCurve();
	}
}

#if WITH_EDITOR
EDataValidationResult FArcScalableFloat::IsDataValid(FDataValidationContext& Context, const FString& PathName) const
{
	switch (Source)
	{
	case EArcScalableFloatSource::None:
	case EArcScalableFloatSource::InlineCurve:
		return EDataValidationResult::Valid;

	case EArcScalableFloatSource::CurveTableRow:
	{
		if (!Curve.CurveTable)
		{
			Context.AddError(FText::Format(LOCTEXT("CurveTableMissing", "{0}: Source is CurveTableRow but no CurveTable is set"), FText::FromString(PathName)));
			return EDataValidationResult::Invalid;
		}
		if (Curve.RowName.IsNone())
		{
			Context.AddError(FText::Format(LOCTEXT("RowNameIsNone", "{0}: CurveTable Row is None"), FText::FromString(PathName)));
			return EDataValidationResult::Invalid;
		}
		if (!Curve.CurveTable->FindCurve(Curve.RowName, PathName, false))
		{
			Context.AddError(FText::Format(LOCTEXT("RowNameDoesNotExist", "{0}: RowName {1} does not exist in CurveTable {2}"),
				FText::FromString(PathName), FText::FromString(Curve.RowName.ToString()), FText::FromString(Curve.CurveTable.GetName())));
			return EDataValidationResult::Invalid;
		}
		return EDataValidationResult::Valid;
	}

	case EArcScalableFloatSource::DataRegistryRow:
	{
		if (!RegistryType.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("RegistryTypeInvalid", "{0}: Source is DataRegistryRow but RegistryType is invalid"), FText::FromString(PathName)));
			return EDataValidationResult::Invalid;
		}
		if (Curve.RowName.IsNone())
		{
			Context.AddError(FText::Format(LOCTEXT("RegistryRowNameIsNone", "{0}: Registry RowName is None"), FText::FromString(PathName)));
			return EDataValidationResult::Invalid;
		}

		const UDataRegistrySubsystem* SubSystem = UDataRegistrySubsystem::Get();
		if (!SubSystem->IsConfigEnabled(false) || !SubSystem->AreRegistriesInitialized())
		{
			if (Context.GetValidationUsecase() != EDataValidationUsecase::None)
			{
				Context.AddWarning(FText::Format(LOCTEXT("DataRegistriesNotLoaded", "{0}: DataRegistries were not loaded in time for validation"), FText::FromString(PathName)));
			}
			return EDataValidationResult::NotValidated;
		}

		float OutValue = 0.f;
		const FRealCurve* OutCurve = nullptr;
		const FDataRegistryId DataRegistryId(RegistryType, Curve.RowName);
		const FDataRegistryCacheGetResult GetCurveResult = SubSystem->EvaluateCachedCurve(OutValue, OutCurve, DataRegistryId, OutValue);
		if (!GetCurveResult)
		{
			const FText WarningText = FText::Format(LOCTEXT("DataRegistryCurveMissing", "{0}: Specified DataRegistry Curve '{1}' does not exist (or was not loaded)"),
				FText::FromString(PathName), DataRegistryId.ToText());
			switch (Context.GetValidationUsecase())
			{
			case EDataValidationUsecase::Manual:
				Context.AddWarning(WarningText);
				return EDataValidationResult::NotValidated;
			default:
				Context.AddMessage(EMessageSeverity::Info, WarningText);
				return EDataValidationResult::NotValidated;
			}
		}
		return EDataValidationResult::Valid;
	}

	case EArcScalableFloatSource::InlineRange:
	{
		if (MaxValue < MinValue)
		{
			Context.AddError(FText::Format(LOCTEXT("InlineRangeInverted", "{0}: InlineRange MaxValue ({1}) is less than MinValue ({2})"),
				FText::FromString(PathName), FText::AsNumber(MaxValue), FText::AsNumber(MinValue)));
			return EDataValidationResult::Invalid;
		}
		if (CustomScaling.IsValid())
		{
			Context.AddMessage(EMessageSeverity::Info,
				FText::Format(LOCTEXT("RangeWithCustomScaling", "{0}: Source is InlineRange but CustomScaling is also set — CustomScaling is ignored for range sources"),
					FText::FromString(PathName)));
		}
		return EDataValidationResult::Valid;
	}

	case EArcScalableFloatSource::DataTableRange:
	{
		if (!DataTable.DataTable)
		{
			Context.AddError(FText::Format(LOCTEXT("DataTableRangeMissing", "{0}: Source is DataTableRange but no DataTable is set"), FText::FromString(PathName)));
			return EDataValidationResult::Invalid;
		}
		if (DataTable.RowName.IsNone())
		{
			Context.AddError(FText::Format(LOCTEXT("DataTableRangeRowNone", "{0}: DataTableRange RowName is None"), FText::FromString(PathName)));
			return EDataValidationResult::Invalid;
		}
		if (DataTable.DataTable->GetRowStruct() != FArcScalableFloatRange::StaticStruct())
		{
			Context.AddError(FText::Format(LOCTEXT("DataTableRangeWrongStruct", "{0}: DataTable {1} row struct is {2}, expected FArcScalableFloatRange"),
				FText::FromString(PathName),
				FText::FromString(DataTable.DataTable->GetPathName()),
				FText::FromString(DataTable.DataTable->GetRowStruct()->GetName())));
			return EDataValidationResult::Invalid;
		}
		if (!DataTable.GetRow<FArcScalableFloatRange>(PathName))
		{
			Context.AddError(FText::Format(LOCTEXT("DataTableRangeRowMissing", "{0}: Row '{1}' does not exist in DataTable {2}"),
				FText::FromString(PathName),
				FText::FromString(DataTable.RowName.ToString()),
				FText::FromString(DataTable.DataTable->GetPathName())));
			return EDataValidationResult::Invalid;
		}
		if (CustomScaling.IsValid())
		{
			Context.AddMessage(EMessageSeverity::Info,
				FText::Format(LOCTEXT("DataTableRangeWithCustomScaling", "{0}: Source is DataTableRange but CustomScaling is also set — CustomScaling is ignored for range sources"),
					FText::FromString(PathName)));
		}
		return EDataValidationResult::Valid;
	}

	case EArcScalableFloatSource::DataRegistryRange:
	{
		if (!RegistryType.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("DataRegistryRangeTypeInvalid", "{0}: Source is DataRegistryRange but RegistryType is invalid"), FText::FromString(PathName)));
			return EDataValidationResult::Invalid;
		}
		if (RangeRowName.IsNone())
		{
			Context.AddError(FText::Format(LOCTEXT("DataRegistryRangeRowNone", "{0}: Source is DataRegistryRange but RangeRowName is None"), FText::FromString(PathName)));
			return EDataValidationResult::Invalid;
		}
		if (CustomScaling.IsValid())
		{
			Context.AddMessage(EMessageSeverity::Info,
				FText::Format(LOCTEXT("DataRegistryRangeWithCustomScaling", "{0}: Source is DataRegistryRange but CustomScaling is also set — CustomScaling is ignored for range sources"),
					FText::FromString(PathName)));
		}
		// Runtime registry availability cannot be guaranteed at validation time; skip live lookup.
		return EDataValidationResult::Valid;
	}
	}

	return EDataValidationResult::Valid;
}
#endif

bool FArcScalableFloat::operator==(const FArcScalableFloat& Other) const
{
	// InlineCurve is compared by key count only; FRichCurve has no operator==. Deep curve
	// equality is rarely needed and the property system uses reflection-based diffing for
	// change detection.
	return Value == Other.Value
		&& Source == Other.Source
		&& Curve == Other.Curve
		&& RegistryType == Other.RegistryType
		&& InlineCurve.GetNumKeys() == Other.InlineCurve.GetNumKeys()
		&& MinValue == Other.MinValue
		&& MaxValue == Other.MaxValue
		&& DataTable == Other.DataTable
		&& RangeRowName == Other.RangeRowName;
}

bool FArcScalableFloat::operator!=(const FArcScalableFloat& Other) const
{
	return !(*this == Other);
}

#undef LOCTEXT_NAMESPACE
