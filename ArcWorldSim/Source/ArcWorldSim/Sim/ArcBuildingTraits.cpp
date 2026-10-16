// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcBuildingTraits.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Mass/EntityUtils.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassWorldUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcBuildingTraits)

namespace ArcBuildingTraitsPrivate
{
void AddLabel(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World,
	const FArcWorldSimLabelFragment& Label)
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct LabelStruct = EntityManager.GetOrCreateConstSharedFragment(Label);
	BuildContext.AddConstSharedFragment(LabelStruct);
}

bool IsSupportedFunctionType(const UScriptStruct* Type)
{
	return Type == FArcProductionConfigFragment::StaticStruct()
		|| Type == FArcGatheringConfigFragment::StaticStruct()
		|| Type == FArcStorageConfigFragment::StaticStruct()
		|| Type == FArcItemSourceConfigFragment::StaticStruct()
		|| Type == FArcItemConsumerConfigFragment::StaticStruct()
		|| Type == FArcServiceConfigFragment::StaticStruct()
		|| Type == FArcStaffingConfigFragment::StaticStruct();
}
}

bool UArcBuildingTrait::ValidateFunctionConfigs(FString& OutError) const
{
	if (FunctionConfigs.IsEmpty())
	{
		OutError = TEXT("FunctionConfigs must contain at least one building function");
		return false;
	}

	TSet<const UScriptStruct*> SeenTypes;
	for (int32 ConfigIndex = 0; ConfigIndex < FunctionConfigs.Num(); ++ConfigIndex)
	{
		const FInstancedStruct& FunctionConfig = FunctionConfigs[ConfigIndex];
		const UScriptStruct* Type = FunctionConfig.GetScriptStruct();
		if (!FunctionConfig.IsValid() || !Type)
		{
			OutError = FString::Printf(TEXT("FunctionConfigs[%d] is empty"), ConfigIndex);
			return false;
		}
		if (!ArcBuildingTraitsPrivate::IsSupportedFunctionType(Type))
		{
			OutError = FString::Printf(TEXT("FunctionConfigs[%d] has unsupported type '%s'"), ConfigIndex, *Type->GetPathName());
			return false;
		}
		if (SeenTypes.Contains(Type))
		{
			OutError = FString::Printf(TEXT("FunctionConfigs contains duplicate type '%s'"), *Type->GetName());
			return false;
		}
		SeenTypes.Add(Type);

		if (const FArcProductionConfigFragment* Config = FunctionConfig.GetPtr<FArcProductionConfigFragment>())
		{
			if (Config->Recipes.IsEmpty() || Config->InputCapacity <= 0 || Config->OutputCapacity <= 0)
			{
				OutError = TEXT("Production requires recipes and positive capacities");
				return false;
			}
			for (const FArcNamedPrimaryAssetId& RecipeId : Config->Recipes)
			{
				if (ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId.AssetId) == nullptr)
				{
					OutError = TEXT("Production contains an invalid recipe");
					return false;
				}
			}
		}
		else if (const FArcGatheringConfigFragment* GatheringConfig = FunctionConfig.GetPtr<FArcGatheringConfigFragment>())
		{
			if (!GatheringConfig->AreaTag.IsValid() || GatheringConfig->OutputCapacity <= 0)
			{
				OutError = TEXT("Gathering requires an area tag and positive output capacity");
				return false;
			}
		}
		else if (const FArcStorageConfigFragment* StorageConfig = FunctionConfig.GetPtr<FArcStorageConfigFragment>())
		{
			if (StorageConfig->Capacity <= 0)
			{
				OutError = TEXT("Storage capacity must be positive");
				return false;
			}
		}
		else if (const FArcItemSourceConfigFragment* ItemSourceConfig = FunctionConfig.GetPtr<FArcItemSourceConfigFragment>())
		{
			const FPrimaryAssetId ItemDefinition = ItemSourceConfig->ItemDefinition;
			if (!ItemDefinition.IsValid() || ItemSourceConfig->AmountPerInteraction <= 0)
			{
				OutError = TEXT("Item source requires an item definition and positive interaction amount");
				return false;
			}
		}
		else if (const FArcItemConsumerConfigFragment* ItemConsumerConfig = FunctionConfig.GetPtr<FArcItemConsumerConfigFragment>())
		{
			if (ItemConsumerConfig->WantedItems.IsEmpty() || ItemConsumerConfig->InputCapacity <= 0)
			{
				OutError = TEXT("Item consumer requires wanted items and positive input capacity");
				return false;
			}
			for (const FArcNamedPrimaryAssetId& WantedItem : ItemConsumerConfig->WantedItems)
			{
				const FPrimaryAssetId ItemDefinition = WantedItem;
				if (!ItemDefinition.IsValid())
				{
					OutError = TEXT("Item consumer contains an invalid wanted item");
					return false;
				}
			}
		}
		else if (const FArcServiceConfigFragment* ServiceConfig = FunctionConfig.GetPtr<FArcServiceConfigFragment>())
		{
			if (ServiceConfig->SlotTags.IsEmpty())
			{
				OutError = TEXT("Service requires at least one slot tag");
				return false;
			}
		}
	}

	const bool bHasStaffing = SeenTypes.Contains(FArcStaffingConfigFragment::StaticStruct());
	if (SeenTypes.Contains(FArcProductionConfigFragment::StaticStruct()) && !bHasStaffing)
	{
		OutError = TEXT("Production requires Staffing because production processors consume Staffing-emitted fragments");
		return false;
	}
	if (SeenTypes.Contains(FArcGatheringConfigFragment::StaticStruct()) && !bHasStaffing)
	{
		OutError = TEXT("Gathering requires Staffing because gathering processors consume Staffing-emitted fragments");
		return false;
	}
	return true;
}

void UArcBuildingTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FString ValidationError;
	if (!ValidateFunctionConfigs(ValidationError))
	{
		UE_LOG(LogMass, Error, TEXT("UArcBuildingTrait on '%s': %s"), *GetPathName(), *ValidationError);
		return;
	}

	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FArcSmartObjectOwnerFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	BuildContext.AddTag<FArcBuildingTag>();
	FArcWorldSimLabelFragment LabelFragment;
	LabelFragment.DisplayName = Label.ToString();
	ArcBuildingTraitsPrivate::AddLabel(BuildContext, World, LabelFragment);

	for (const FInstancedStruct& FunctionConfig : FunctionConfigs)
	{
		const UScriptStruct* Type = FunctionConfig.GetScriptStruct();
		check(Type && FunctionConfig.GetMemory());
		const FConstSharedStruct& ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(*Type, FunctionConfig.GetMemory());
		BuildContext.AddConstSharedFragment(ConfigFragment);

		if (Type == FArcProductionConfigFragment::StaticStruct())
		{
			BuildContext.AddFragment<FArcProductionSlotsFragment>();
		}
		else if (Type == FArcGatheringConfigFragment::StaticStruct())
		{
			BuildContext.AddFragment<FArcGatheringStateFragment>();
		}
		else if (Type == FArcStaffingConfigFragment::StaticStruct())
		{
			BuildContext.AddFragment<FArcStaffedFragment>();
			BuildContext.AddFragment<FArcEmploymentCountFragment>();
		}
	}
}

bool UArcBuildingTrait::ValidateTemplate(
	const FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World,
	FAdditionalTraitRequirements& OutTraitRequirements) const
{
	FString ValidationError;
	if (!ValidateFunctionConfigs(ValidationError))
	{
		UE_LOG(LogMass, Error, TEXT("UArcBuildingTrait on '%s': %s"), *GetPathName(), *ValidationError);
		return false;
	}
	return true;
}

void UArcGatherAreaTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FArcSmartObjectOwnerFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(Config);
	BuildContext.AddConstSharedFragment(ConfigFragment);
	ArcBuildingTraitsPrivate::AddLabel(BuildContext, World, Label);
}

void UArcSettlementCenterTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment_GetRef<FArcSettlementCenterFragment>() = Center;
	ArcBuildingTraitsPrivate::AddLabel(BuildContext, World, Label);
}
