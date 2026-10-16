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

#include "ArcFrontendMVVMModelComponent.h"

#include "MVVM/ArcViewModelSlotWidgetInterface.h"
#include "ArcCoreGameplayTags.h"
#include "MVVMGameSubsystem.h"
#include "MVVMViewModelBase.h"
#include "Blueprint/UserWidget.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "StructUtils/InstancedStruct.h"
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"
#include "GameFramework/ArcActorFeatureUtils.h"
#include "Player/ArcMassPlayerStateExtensionComponent.h"
#include "Core/ArcMassGameFrameworkTags.h"

UObject* UArcFrontendMVVMComponentResolver::CreateInstance(const UClass* /*ExpectedType*/,
	const UUserWidget* UserWidget, const UMVVMView* /*View*/) const
{
	if (!UserWidget) { return nullptr; }
	APlayerState* PS = UserWidget->GetOwningPlayerState();
	if (!PS) { return nullptr; }
	UArcFrontendMVVMModelComponent* Comp = PS->FindComponentByClass<UArcFrontendMVVMModelComponent>();
	if (!Comp) { return nullptr; }
	UMVVMViewModelBase* ViewModel = nullptr;
	Comp->FindViewModel(ViewModelTag, ViewModel);
	return ViewModel;
}

void UArcFrontendMVVMModel::NaiveBeginPlay()
{
}

void UArcFrontendMVVMModel::NativeTick(float DeltaTime)
{
	Tick(DeltaTime);
}

AActor* UArcFrontendMVVMModel::GetOwnerActor() const
{
	return GetTypedOuter<UArcFrontendMVVMModelComponent>()->GetOwner();
}

APlayerState* UArcFrontendMVVMModel::GetOwnerPlayerState() const
{
	if (APlayerState* PS = Cast<APlayerState>(GetOwnerActor()))
	{
		return PS;
	}
	
	if (APlayerController* PC = Cast<APlayerController>(GetOwnerActor()))
	{
		return PC->PlayerState;
	}

	return nullptr;
}

APlayerController* UArcFrontendMVVMModel::GetOwnerPlayerController() const
{
	if (APlayerState* PS = Cast<APlayerState>(GetOwnerActor()))
	{
		return PS->GetPlayerController();
	}
	
	if (APlayerController* PC = Cast<APlayerController>(GetOwnerActor()))
	{
		return PC;
	}

	return nullptr;
}

UActorComponent* UArcFrontendMVVMModel::GetComponentByClass(TSubclassOf<UActorComponent> ComponentClass) const
{
	return GetOwnerActor()->GetComponentByClass(ComponentClass);
}

UArcFrontendMVVMModelComponent* UArcFrontendMVVMModel::GetMVVMComponent() const
{
	return GetTypedOuter<UArcFrontendMVVMModelComponent>();
}

UWorld* UArcFrontendMVVMModel::GetWorld() const
{
	UArcFrontendMVVMModelComponent* Comp = GetTypedOuter<UArcFrontendMVVMModelComponent>();
	if (!Comp)
	{
		return nullptr;
	}
	
	return Comp->GetWorld();
}

void FArcFrontendMVVMStructModel::CreateViewModels(UArcFrontendMVVMModelComponent& Component)
{
	// Default: no view models. Derived structs override to push their VMs.
}

void FArcFrontendMVVMStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
{
}

void FArcFrontendMVVMStructModel::Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component)
{
}

void FArcFrontendMVVMStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
{
}

UMVVMViewModelCollectionObject* UArcFrontendMVVMModelComponent::GetGlobalViewModelCollection() const
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI)
	{
		return nullptr;
	}

	UMVVMGameSubsystem* MVVMSubsystem = GI->GetSubsystem<UMVVMGameSubsystem>();
	if (!MVVMSubsystem)
	{
		return nullptr;
	}

	return MVVMSubsystem->GetViewModelCollection();
}

UArcFrontendMVVMModelComponent::UArcFrontendMVVMModelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UArcFrontendMVVMModelComponent::BeginPlay()
{
	Super::BeginPlay();

	// No UI view-models on a headless server.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	InitStateHandle = ArcActorFeatureUtils::RegisterFeatureInitStateDelegate<UArcMassPlayerStateExtensionComponent>(
		GetOwner(),
		ArcActorFeatureUtils::TFeatureDelegate<UArcMassPlayerStateExtensionComponent>::CreateUObject(
			this, &UArcFrontendMVVMModelComponent::OnPlayerStateDataInitialized),
		FArcMassGameFrameworkTags::Get().InitState_DataInitialized,
		/*bCallImmediately*/ true,
		/*bCallOnce*/        true);
}

void UArcFrontendMVVMModelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ArcActorFeatureUtils::UnregisterFeatureInitStateDelegate(GetOwner(), InitStateHandle);

	for (FInstancedStruct& Entry : StructModels)
	{
		FArcFrontendMVVMStructModel* StructModel = Entry.GetMutablePtr<FArcFrontendMVVMStructModel>();
		if (StructModel)
		{
			StructModel->Deinitialize(*this);
		}
	}

	if (UMVVMViewModelCollectionObject* Collection = GetGlobalViewModelCollection())
	{
		for (const FMVVMViewModelContext& Ctx : RegisteredGlobalContexts)
		{
			Collection->RemoveViewModel(Ctx);
		}
	}
	RegisteredGlobalContexts.Empty();

	Super::EndPlay(EndPlayReason);
}

void UArcFrontendMVVMModelComponent::OnPlayerStateDataInitialized(UArcMassPlayerStateExtensionComponent* /*PSExt*/)
{
	// UI view-models exist only for the local player. The global MVVM collection is a
	// GameInstance singleton shared by every PlayerState's component; registering from
	// remote PlayerStates collides on ContextName and makes EndPlay removal unsafe.
	APlayerState* PS = GetOwner<APlayerState>();
	if (!PS || !PS->GetPlayerController() || !PS->GetPlayerController()->IsLocalController())
	{
		return;
	}

	for (TSubclassOf<UArcFrontendMVVMModel> ModelClass : ModelClasses)
	{
		UArcFrontendMVVMModel* ModelInstance = NewObject<UArcFrontendMVVMModel>(this, ModelClass);
		ModelInstance->BeginPlay();
		ModelInstance->NaiveBeginPlay();

		ModelInstances.Add(ModelInstance);
		if (ModelInstance->bTickForAll)
		{
			ModelTickForAll.Add(ModelInstance);
		}

		if (ModelInstance->bTickForLocalOnly)
		{
			ModelTickLocal.Add(ModelInstance);
		}
	}

	for (int32 Idx = 0; Idx < StructModels.Num(); ++Idx)
	{
		FInstancedStruct& Entry = StructModels[Idx];
		FArcFrontendMVVMStructModel* StructModel = Entry.GetMutablePtr<FArcFrontendMVVMStructModel>();
		if (!StructModel)
		{
			continue;
		}

		StructModel->CreateViewModels(*this);
		StructModel->Initialize(*this);

		if (StructModel->bTickForAll)
		{
			StructTickForAllIndices.Add(Idx);
		}
		if (StructModel->bTickForLocalOnly)
		{
			StructTickLocalIndices.Add(Idx);
		}
	}

	bool bNeedsTick = !ModelTickForAll.IsEmpty()
		|| !ModelTickLocal.IsEmpty()
		|| !StructTickForAllIndices.IsEmpty()
		|| !StructTickLocalIndices.IsEmpty();
	SetComponentTickEnabled(bNeedsTick);
}

void UArcFrontendMVVMModelComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (APlayerState* PS = GetOwner<APlayerState>())
	{
		if (PS->GetPlayerController())
		{
			for (UArcFrontendMVVMModel* Model : ModelTickLocal)
			{
				Model->Tick(DeltaTime);
			}
		}
	}
	
	for (UArcFrontendMVVMModel* Model : ModelTickForAll)
	{
		Model->Tick(DeltaTime);
	}

	if (APlayerState* StructPS = GetOwner<APlayerState>())
	{
		if (StructPS->GetPlayerController())
		{
			for (int32 Idx : StructTickLocalIndices)
			{
				FArcFrontendMVVMStructModel* StructModel = StructModels[Idx].GetMutablePtr<FArcFrontendMVVMStructModel>();
				if (StructModel)
				{
					StructModel->Tick(DeltaTime, *this);
				}
			}
		}
	}

	for (int32 Idx : StructTickForAllIndices)
	{
		FArcFrontendMVVMStructModel* StructModel = StructModels[Idx].GetMutablePtr<FArcFrontendMVVMStructModel>();
		if (StructModel)
		{
			StructModel->Tick(DeltaTime, *this);
		}
	}
}

bool UArcFrontendMVVMModelComponent::FindViewModel(FGameplayTag ViewModelTag, UMVVMViewModelBase*& ViewModel)
{
	if (ViewModels.Contains(ViewModelTag))
	{
		ViewModel = ViewModels[ViewModelTag];
		return true;
	}

	ViewModel = nullptr;
	return false;
}

void UArcFrontendMVVMModelComponent::AddViewModel(FGameplayTag ViewModelTag
	, UMVVMViewModelBase* InViewModel)
{
	ViewModels.FindOrAdd(ViewModelTag) = InViewModel;
}

void UArcFrontendMVVMModelComponent::RemoveViewModel(FGameplayTag ViewModelTag)
{
	ViewModels.Remove(ViewModelTag);
}

void UArcFrontendMVVMModelComponent::RegisterViewModel(UMVVMViewModelBase* VM, FName ContextName, FGameplayTag RegistrationTag)
{
	if (!VM)
	{
		return;
	}

	if (!ContextName.IsNone())
	{
		UMVVMViewModelCollectionObject* Collection = GetGlobalViewModelCollection();
		if (Collection)
		{
			FMVVMViewModelContext Ctx;
			Ctx.ContextClass = VM->GetClass();
			Ctx.ContextName = ContextName;
			if (Collection->AddViewModelInstance(Ctx, VM))
			{
				RegisteredGlobalContexts.Add(Ctx);
			}
		}
	}

	if (RegistrationTag.IsValid())
	{
		AddViewModel(RegistrationTag, VM);
	}
}

void UArcFrontendMVVMModelComponent::CreateAndRegisterViewModels(TConstArrayView<FArcViewModelSpec> Specs)
{
	for (const FArcViewModelSpec& Spec : Specs)
	{
		if (!Spec.ViewModelClass)
		{
			continue;
		}

		UMVVMViewModelBase* VM = NewObject<UMVVMViewModelBase>(this, Spec.ViewModelClass);
		RegisterViewModel(VM, Spec.ContextName, Spec.RegistrationTag);
	}
}

UMVVMViewModelBase* UArcFrontendMVVMModelComponent::FindViewModelByContext(FName ContextName, TSubclassOf<UMVVMViewModelBase> Class) const
{
	UMVVMViewModelCollectionObject* Collection = GetGlobalViewModelCollection();
	if (!Collection)
	{
		return nullptr;
	}

	FMVVMViewModelContext Ctx;
	Ctx.ContextClass = Class;
	Ctx.ContextName = ContextName;
	return Collection->FindViewModelInstance(Ctx);
}

UObject* UArcFrontendMVVMWidgetSlotResolver::CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* /*View*/) const
{
	if (!UserWidget || !UserWidget->Implements<UArcViewModelSlotWidget>())
	{
		return nullptr;
	}

	const FGameplayTag SlotTag = IArcViewModelSlotWidget::Execute_GetViewModelSlotTag(const_cast<UUserWidget*>(UserWidget));
	if (!SlotTag.IsValid())
	{
		return nullptr;
	}

	APlayerState* PS = UserWidget->GetOwningPlayerState();
	if (!PS)
	{
		return nullptr;
	}

	UArcFrontendMVVMModelComponent* Comp = PS->FindComponentByClass<UArcFrontendMVVMModelComponent>();
	if (!Comp)
	{
		return nullptr;
	}

	UMVVMViewModelBase* ViewModel = nullptr;
	Comp->FindViewModel(SlotTag, ViewModel);

	// Guard the engine's IsA(ExpectedType) contract explicitly: the tag map is
	// untyped (UMVVMViewModelBase*), so a misconfigured binding could resolve a
	// VM whose class != the widget's declared source type. Return null + log
	// rather than hand back a wrong-type VM the engine would silently reject.
	if (ViewModel && ExpectedType && !ViewModel->IsA(ExpectedType))
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcFrontend slot resolver: VM '%s' for tag '%s' is not a '%s'"),
			*ViewModel->GetClass()->GetName(), *SlotTag.ToString(), *ExpectedType->GetName());
		return nullptr;
	}

	return ViewModel;
}