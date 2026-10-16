#pragma once

#include "Decision/ArcTacticsCompiledData.h"
#include "Decision/ArcTacticsConfig.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"

namespace ArcTacticsSlotTestHelpers
{
	inline bool InitializeSlot(
		FArcTacticsInstanceSlot& Slot,
		const FArcTacticsCompiledData& CompiledData,
		const FArcTacticsDecisionSettings& Settings,
		const FMassEntityHandle Entity,
		const int32 RandomSeed,
		const UArcTacticsConfig* Config = nullptr)
	{
		if (!Slot.Initialize(Entity, Settings, RandomSeed))
		{
			return false;
		}
		const TSharedPtr<const FArcTacticsCompiledData> Definitions(
			&CompiledData, [](const FArcTacticsCompiledData*) {});
		return Slot.AddPackage(Config, Definitions).bActive;
	}

	inline FArcTacticsPackage& Package(FArcTacticsInstanceSlot& Slot, const int32 PackageIndex = 0)
	{
		return *Slot.Packages[PackageIndex];
	}

	inline FArcTacticsInstanceData& PackageInstance(FArcTacticsInstanceSlot& Slot, const int32 PackageIndex = 0)
	{
		return Slot.Packages[PackageIndex]->Instance;
	}

	inline void BindActiveActionToPackage(FArcTacticsInstanceSlot& Slot, const int32 PackageIndex = 0)
	{
		Slot.ActiveAction.PackageIndex = PackageIndex;
		Slot.ActiveAction.PackageSerial = Slot.Packages[PackageIndex]->Serial;
	}

	inline TArray<TObjectPtr<const UArcTacticsConfig>> Configs(const UArcTacticsConfig& Config)
	{
		TArray<TObjectPtr<const UArcTacticsConfig>> Result;
		Result.Add(&Config);
		return Result;
	}

	inline FArcTacticsCooldownKey CooldownKey(const UArcTacticsConfig& Config, const int32 BranchIndex)
	{
		return FArcTacticsCooldownKey(&Config, BranchIndex);
	}
}
