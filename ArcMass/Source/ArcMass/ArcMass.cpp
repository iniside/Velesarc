// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass.h"

#include "Misc/CoreDelegates.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementHierarchyInterface.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementWorldInterface.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementSelectionInterface.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Mesh/ArcRenderMeshFragments.h"

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
		Registry->RegisterElementType<FArcMassEntityElementData, true>(NAME_ArcMassEntity);
		Registry->RegisterElementInterface<ITypedElementHierarchyInterface>(NAME_ArcMassEntity, NewObject<UArcMassEntityElementHierarchyInterface>());
		Registry->RegisterElementInterface<ITypedElementWorldInterface>(NAME_ArcMassEntity, NewObject<UArcMassEntityElementWorldInterface>());
		Registry->RegisterElementInterface<ITypedElementSelectionInterface>(NAME_ArcMassEntity, NewObject<UArcMassEntityElementSelectionInterface>());
	}

#if WITH_EDITOR
	// Invert the ArcMassRender -> ArcMass dependency: ArcMassRender hit proxies resolve their
	// element handle on demand through this delegate, which only ArcMass can satisfy (it owns the
	// typed-element owner store and the concrete partition-actor cast).
	GetArcResolveTypeElementHandleDelegate().BindStatic(&ArcMassEntityElementUtils::ResolveRenderInstanceId);
	EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddStatic(&ArcMassEntityElementUtils::DrainEditorArcMassEntityElementOwnerStore);
#endif
}

void FArcMassModule::ShutdownModule()
{
#if WITH_EDITOR
	GetArcResolveTypeElementHandleDelegate().Unbind();
	FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitHandle);
#endif

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
