// Copyright Lukasz Baran. All Rights Reserved.

#include "Decision/ArcTacticsCompiledData.h"

#include "ArcTQSPropertyFunction.h"
#include "PropertyBindingDataView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsCompiledData)

bool FArcTacticsSelectionSettings::IsValid() const
{
	return StaticEnum<EArcTacticsSelectionMode>()->IsValidEnumValue(static_cast<int64>(Mode))
		&& FMath::IsFinite(TopPercent)
		&& TopPercent >= 0.0f
		&& TopPercent <= 1.0f;
}

bool FArcTacticsDecisionSettings::IsValid() const
{
	return FMath::IsFinite(DecisionInterval)
		&& DecisionInterval >= 0.0f
		&& Selection.IsValid()
		&& HistorySize >= 1;
}

bool FArcTacticsDecisionSettingsParameters::IsValid() const
{
	return FMath::IsFinite(MinScore) && MinScore >= 0.0f;
}

bool FArcTacticsBranchParameters::IsValid() const
{
	return FMath::IsFinite(Weight)
		&& Weight >= 0.0f
		&& FMath::IsFinite(Cooldown)
		&& Cooldown >= 0.0f
		&& StaticEnum<EArcTacticsCooldownStart>()->IsValidEnumValue(static_cast<int64>(CooldownStart));
}

void FArcTacticsCompiledData::Reset()
{
	Version = 0;
	Revision = 0;
	bIsValid = false;
	DefaultDecisionSettings = FArcTacticsDecisionSettingsParameters();
	DefaultGlobalParameters.Reset();
	GlobalInitializationBindingsBatch = FPropertyBindingIndex16();
	DecisionSettingsBindingsBatch = FPropertyBindingIndex16();
	DefaultInstanceData.Reset();
	Nodes.Reset();
	GlobalTasks.Reset();
	Actions.Reset();
	Selectors.Reset();
	Functions.Reset();
	Bindings.Reset();
}

namespace ArcTacticsCompiledDataPrivate
{
	void PatchBagStruct(
		FArcTacticsBindingCollection& Bindings,
		const FArcTacticsDataHandle Handle,
		const FPropertyBindingIndex16 InitializationBatch,
		const FInstancedPropertyBag& Bag)
	{
		const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct();
		if (FArcTacticsBindableStructDescriptor* Descriptor = Bindings.FindMutableBindableStruct(Handle))
		{
			Descriptor->Struct = BagStruct;
		}
		TArray<FPropertyBindingCopyInfoBatch>& CopyBatches = Bindings.GetMutableCopyBatches();
		if (InitializationBatch.IsValid() && CopyBatches.IsValidIndex(InitializationBatch.Get()))
		{
			CopyBatches[InitializationBatch.Get()].TargetStruct.GetMutable().Struct = BagStruct;
		}
	}

	FPropertyBindingDataView GetDefaultDataView(FArcTacticsCompiledData& Data, const FArcTacticsDataHandle Handle)
	{
		switch (Handle.GetKind())
		{
		case EArcTacticsBindingEndpointKind::Node:
			return Data.DefaultInstanceData.IsValidIndex(Handle.GetIndex())
				? FPropertyBindingDataView(Data.DefaultInstanceData[Handle.GetIndex()])
				: FPropertyBindingDataView();
		case EArcTacticsBindingEndpointKind::DecisionSettings:
			return FPropertyBindingDataView(FArcTacticsDecisionSettingsParameters::StaticStruct(), &Data.DefaultDecisionSettings);
		case EArcTacticsBindingEndpointKind::GlobalParameters:
			return FPropertyBindingDataView(Data.DefaultGlobalParameters.GetMutableValue());
		case EArcTacticsBindingEndpointKind::Function:
			return Data.Functions.IsValidIndex(Handle.GetIndex())
				? FPropertyBindingDataView(Data.Functions[Handle.GetIndex()].Function)
				: FPropertyBindingDataView();
		default:
			break;
		}
		for (FArcTacticsCompiledAction& Action : Data.Actions)
		{
			if (Action.Parameters == Handle)
			{
				return FPropertyBindingDataView(FArcTacticsBranchParameters::StaticStruct(), &Action.DefaultParameters);
			}
			if (Action.ParameterBag == Handle)
			{
				return FPropertyBindingDataView(Action.DefaultParameterBag.GetMutableValue());
			}
		}
		for (FArcTacticsCompiledSelector& Selector : Data.Selectors)
		{
			if (Selector.Parameters == Handle)
			{
				return FPropertyBindingDataView(FArcTacticsBranchParameters::StaticStruct(), &Selector.DefaultParameters);
			}
			if (Selector.ParameterBag == Handle)
			{
				return FPropertyBindingDataView(Selector.DefaultParameterBag.GetMutableValue());
			}
		}
		return FPropertyBindingDataView();
	}

	bool UpdateBindingSegments(FArcTacticsCompiledData& Data)
	{
		bool bUpdated = true;
		Data.Bindings.ForEachMutableBinding([&Data, &bUpdated](FPropertyBindingBinding& Binding)
		{
			const FArcTacticsBinding& ArcBinding = static_cast<const FArcTacticsBinding&>(Binding);
			bUpdated = ArcTacticsBindings::UpdatePathSegments(
					Binding.GetMutableSourcePath(), GetDefaultDataView(Data, ArcBinding.GetSourceDataHandle()))
				&& ArcTacticsBindings::UpdatePathSegments(
					Binding.GetMutableTargetPath(), GetDefaultDataView(Data, ArcBinding.GetTargetDataHandle()))
				&& bUpdated;
		});
		return bUpdated;
	}
}

bool FArcTacticsCompiledData::ResolvePaths()
{
	if (!bIsValid)
	{
		return false;
	}
	for (const FArcTacticsCompiledFunction& Function : Functions)
	{
		if (!Function.Function.GetPtr<FArcTQSPropertyFunction>())
		{
			bIsValid = false;
			return false;
		}
	}
	ArcTacticsCompiledDataPrivate::PatchBagStruct(Bindings,
		FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::GlobalParameters),
		GlobalInitializationBindingsBatch, DefaultGlobalParameters);
	for (const FArcTacticsCompiledAction& Action : Actions)
	{
		ArcTacticsCompiledDataPrivate::PatchBagStruct(Bindings, Action.ParameterBag,
			Action.ParameterBagInitializationBindingsBatch, Action.DefaultParameterBag);
	}
	for (const FArcTacticsCompiledSelector& Selector : Selectors)
	{
		ArcTacticsCompiledDataPrivate::PatchBagStruct(Bindings, Selector.ParameterBag,
			Selector.ParameterBagInitializationBindingsBatch, Selector.DefaultParameterBag);
	}
	bIsValid = ArcTacticsCompiledDataPrivate::UpdateBindingSegments(*this) && Bindings.ResolvePaths();
	return bIsValid;
}

bool FArcTacticsCompiledData::IsExecutable() const
{
	return bIsValid
		&& Version == CurrentVersion
		&& Revision != 0
		&& Bindings.IsValid();
}
