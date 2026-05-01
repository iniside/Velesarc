// Copyright Lukasz Baran. All Rights Reserved.

#include "MassProcessorBrowser/ArcMassProcessorBrowserModel.h"
#include "MassProcessorBrowser/ArcMassProcessorBrowserConfig.h"
#include "MassProcessor.h"
#include "MassObserverProcessor.h"
#include "MassSignalProcessorBase.h"
#include "UObject/Class.h"
#include "Interfaces/IPluginManager.h"

void FArcMassProcessorBrowserModel::Build(const UArcMassProcessorBrowserConfig* Config)
{
	RootItems.Empty();
	ProcessorItemMap.Empty();

	TArray<UClass*> SubClasses;
	GetDerivedClasses(UMassProcessor::StaticClass(), SubClasses);

	SubClasses.Sort([](const UClass& A, const UClass& B)
	{
		return A.GetName() < B.GetName();
	});

	for (UClass* Class : SubClasses)
	{
		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		if (Class == UMassCompositeProcessor::StaticClass())
		{
			continue;
		}

		UMassProcessor* ProcessorCDO = GetMutableDefault<UMassProcessor>(Class);
		if (!ProcessorCDO)
		{
			continue;
		}

		FString CategoryOverride = ResolveCategory(Class, Config);
		TSharedPtr<FArcProcessorTreeItem> ParentItem;

		if (!CategoryOverride.IsEmpty())
		{
			ParentItem = FindOrCreateGroup(RootItems, CategoryOverride, EArcProcessorTreeItemType::Category);
		}
		else
		{
			FString ModuleName = ResolveModuleName(Class);
			FString PluginName = ResolvePluginName(ModuleName);

			TSharedPtr<FArcProcessorTreeItem> PluginItem = FindOrCreateGroup(RootItems, PluginName, EArcProcessorTreeItemType::Plugin);
			if (ModuleName == PluginName)
			{
				// Module has same name as plugin — no nesting, use plugin node directly
				ParentItem = PluginItem;
			}
			else
			{
				ParentItem = FindOrCreateGroup(PluginItem->Children, ModuleName, EArcProcessorTreeItemType::Module);
				ParentItem->Parent = PluginItem;
			}
		}

		// Sub-group by processor type: Signal / Observer / Processor
		FString TypeGroupName;
		if (Class->IsChildOf(UMassSignalProcessorBase::StaticClass()))
		{
			TypeGroupName = TEXT("Signal");
		}
		else if (Class->IsChildOf(UMassObserverProcessor::StaticClass()))
		{
			TypeGroupName = TEXT("Observer");
		}
		else
		{
			TypeGroupName = TEXT("Processor");
		}

		TSharedPtr<FArcProcessorTreeItem> TypeGroup = FindOrCreateGroup(ParentItem->Children, TypeGroupName, EArcProcessorTreeItemType::Category);
		TypeGroup->Parent = ParentItem;

		TSharedPtr<FArcProcessorTreeItem> ProcessorItem = MakeShared<FArcProcessorTreeItem>();
		ProcessorItem->DisplayName = FText::FromString(Class->GetName());
		ProcessorItem->Type = EArcProcessorTreeItemType::Processor;
		ProcessorItem->ProcessorCDO = ProcessorCDO;
		ProcessorItem->Parent = TypeGroup;

		TypeGroup->Children.Add(ProcessorItem);
		ProcessorItemMap.Add(Class, ProcessorItem);
	}
}

FString FArcMassProcessorBrowserModel::ResolveCategory(const UClass* ProcessorClass, const UArcMassProcessorBrowserConfig* Config) const
{
	if (Config)
	{
		const FString* Override = Config->CategoryOverrides.Find(const_cast<UClass*>(ProcessorClass));
		if (Override && !Override->IsEmpty())
		{
			return *Override;
		}
	}

	const FString& CategoryMeta = ProcessorClass->GetMetaData(TEXT("Category"));
	if (!CategoryMeta.IsEmpty())
	{
		return CategoryMeta;
	}

	return FString();
}

FString FArcMassProcessorBrowserModel::ResolveModuleName(const UClass* ProcessorClass) const
{
	// Class package path is /Script/ModuleName — extract the module name
	FString ClassPath = ProcessorClass->GetPathName();
	// GetPathName returns e.g. "/Script/ArcMass.UArcSomeProcessor"
	// We want "ArcMass"
	if (ClassPath.StartsWith(TEXT("/Script/")))
	{
		FString AfterScript = ClassPath.Mid(8); // skip "/Script/"
		int32 DotIndex = INDEX_NONE;
		if (AfterScript.FindChar(TEXT('.'), DotIndex))
		{
			return AfterScript.Left(DotIndex);
		}
		return AfterScript;
	}
	return TEXT("Unknown");
}

FString FArcMassProcessorBrowserModel::ResolvePluginName(const FString& ModuleName) const
{
	TArray<TSharedRef<IPlugin>> Plugins = IPluginManager::Get().GetDiscoveredPlugins();
	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
		for (const FModuleDescriptor& Module : Descriptor.Modules)
		{
			if (Module.Name.ToString() == ModuleName)
			{
				return Descriptor.FriendlyName.IsEmpty() ? Plugin->GetName() : Descriptor.FriendlyName;
			}
		}
	}
	return TEXT("Engine");
}

TSharedPtr<FArcProcessorTreeItem> FArcMassProcessorBrowserModel::FindOrCreateGroup(
	TArray<TSharedPtr<FArcProcessorTreeItem>>& Items,
	const FString& Name,
	EArcProcessorTreeItemType Type)
{
	for (const TSharedPtr<FArcProcessorTreeItem>& Item : Items)
	{
		if (Item->DisplayName.ToString() == Name && Item->Type == Type)
		{
			return Item;
		}
	}

	TSharedPtr<FArcProcessorTreeItem> NewItem = MakeShared<FArcProcessorTreeItem>();
	NewItem->DisplayName = FText::FromString(Name);
	NewItem->Type = Type;
	Items.Add(NewItem);
	return NewItem;
}

TSharedPtr<FArcProcessorTreeItem> FArcMassProcessorBrowserModel::FindItemForProcessor(const UClass* ProcessorClass) const
{
	const TSharedPtr<FArcProcessorTreeItem>* Found = ProcessorItemMap.Find(ProcessorClass);
	return Found ? *Found : nullptr;
}

FArcProcessorRequirementsInfo FArcMassProcessorBrowserModel::ExtractRequirements(UMassProcessor* ProcessorCDO)
{
	FArcProcessorRequirementsInfo Info;
	if (!ProcessorCDO)
	{
		return Info;
	}

	FMassExecutionRequirements Requirements;
	ProcessorCDO->ExportRequirements(Requirements);

#if WITH_STRUCTUTILS_DEBUG
	Requirements.Fragments.Read.DebugGetIndividualNames(Info.FragmentsRead);
	Requirements.Fragments.Write.DebugGetIndividualNames(Info.FragmentsWrite);
	Requirements.SharedFragments.Read.DebugGetIndividualNames(Info.SharedFragmentsRead);
	Requirements.SharedFragments.Write.DebugGetIndividualNames(Info.SharedFragmentsWrite);
	Requirements.ConstSharedFragments.Read.DebugGetIndividualNames(Info.ConstSharedFragmentsRead);
	Requirements.ChunkFragments.Read.DebugGetIndividualNames(Info.ChunkFragmentsRead);
	Requirements.ChunkFragments.Write.DebugGetIndividualNames(Info.ChunkFragmentsWrite);
	Requirements.RequiredSubsystems.Read.DebugGetIndividualNames(Info.SubsystemsRead);
	Requirements.RequiredSubsystems.Write.DebugGetIndividualNames(Info.SubsystemsWrite);
	Requirements.SparseElements.Read.DebugGetIndividualNames(Info.SparseRead);
	Requirements.SparseElements.Write.DebugGetIndividualNames(Info.SparseWrite);
	Requirements.RequiredAllTags.DebugGetIndividualNames(Info.TagsAll);
	Requirements.RequiredAnyTags.DebugGetIndividualNames(Info.TagsAny);
	Requirements.RequiredNoneTags.DebugGetIndividualNames(Info.TagsNone);
#endif

	return Info;
}

bool FArcMassProcessorBrowserModel::HasModifiedConfigProperties(UMassProcessor* ProcessorCDO)
{
	if (!ProcessorCDO)
	{
		return false;
	}

	UClass* Class = ProcessorCDO->GetClass();
	UClass* SuperClass = Class->GetSuperClass();
	if (!SuperClass)
	{
		return false;
	}

	UObject* SuperCDO = SuperClass->GetDefaultObject();

	// Only iterate properties declared on the super class (and above),
	// since SuperCDO is an instance of SuperClass — accessing child-only
	// properties on it would trigger a checkf.
	for (TFieldIterator<FProperty> It(SuperClass); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Config))
		{
			continue;
		}

		const void* CDOValue = Property->ContainerPtrToValuePtr<void>(ProcessorCDO);
		const void* SuperValue = Property->ContainerPtrToValuePtr<void>(SuperCDO);

		if (!Property->Identical(CDOValue, SuperValue))
		{
			return true;
		}
	}

	return false;
}
