// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass.h"

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementHierarchyInterface.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementWorldInterface.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementSelectionInterface.h"
#include "Elements/Framework/TypedElementRegistry.h"

#if WITH_EDITOR
#include "MassEntityConfigAsset.h"
#include "UObject/AssetRegistryTagsContext.h"
#endif

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebuggerCategory_ArcMass.h"
#endif // WITH_GAMEPLAY_DEBUGGER

#define LOCTEXT_NAMESPACE "FArcMassModule"

#if WITH_EDITOR
namespace UE::ArcMass::Persistence
{
	void OnGetMassEntityConfigTags(FAssetRegistryTagsContext Context)
	{
		const UMassEntityConfigAsset* ConfigAsset =
			Cast<UMassEntityConfigAsset>(Context.GetObject());
		if (!ConfigAsset)
		{
			return;
		}

		const FGuid& ConfigGuid = ConfigAsset->GetConfig().GetGuid();
		if (ConfigGuid.IsValid())
		{
			Context.AddTag(UObject::FAssetRegistryTag(
				FName("MassEntityConfigGuid"),
				ConfigGuid.ToString(),
				UObject::FAssetRegistryTag::TT_Hidden));
		}
	}
}
#endif

void FArcMassModule::StartupModule()
{
#if WITH_EDITOR
	UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.AddStatic(
		&UE::ArcMass::Persistence::OnGetMassEntityConfigTags);
#endif

#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("ArcMass", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_ArcMass::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif

	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	if (Registry)
	{
		Registry->RegisterElementType<FSkMInstanceElementData, true>(NAME_SkMInstance);
		Registry->RegisterElementInterface<ITypedElementHierarchyInterface>(NAME_SkMInstance, NewObject<USkMInstanceElementHierarchyInterface>());
		Registry->RegisterElementInterface<ITypedElementWorldInterface>(NAME_SkMInstance, NewObject<USkMInstanceElementWorldInterface>());
		Registry->RegisterElementInterface<ITypedElementSelectionInterface>(NAME_SkMInstance, NewObject<USkMInstanceElementSelectionInterface>());
	}
}

void FArcMassModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("ArcMass");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassModule, ArcMass)
