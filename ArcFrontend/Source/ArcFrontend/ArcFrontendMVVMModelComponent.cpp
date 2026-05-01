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

#include "ArcCoreGameplayTags.h"
#include "MVVMGameSubsystem.h"
#include "MVVMViewModelBase.h"
#include "Blueprint/UserWidget.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Pawn/ArcPawnExtensionComponent.h"
#include "Player/ArcHeroComponentBase.h"
#include "StructUtils/InstancedStruct.h"
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"

UObject* UArcFrontendMVVMComponentResolver::CreateInstance(const UClass* ExpectedType
														   , const UUserWidget* UserWidget
														   , const UMVVMView* View) const
{
	UMVVMViewModelBase* ViewModel = nullptr;
	UserWidget->GetOwningPlayerState()->FindComponentByClass<UArcFrontendMVVMModelComponent>()->FindViewModel(ViewModelTag, ViewModel);

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
	UMVVMViewModelCollectionObject* Collection = Component.GetGlobalViewModelCollection();
	if (!Collection)
	{
		return;
	}

	for (const FArcViewModelSpec& Spec : ViewModelSpecs)
	{
		if (Spec.ContextName.IsNone() || !Spec.ViewModelClass)
		{
			continue;
		}

		UMVVMViewModelBase* VM = NewObject<UMVVMViewModelBase>(&Component, Spec.ViewModelClass);

		FMVVMViewModelContext Context;
		Context.ContextClass = Spec.ViewModelClass;
		Context.ContextName = Spec.ContextName;
		Collection->AddViewModelInstance(Context, VM);

		CachedViewModels.Add(Spec.ContextName, VM);
	}
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

UMVVMViewModelBase* FArcFrontendMVVMStructModel::FindCachedViewModel(FName ContextName) const
{
	const TObjectPtr<UMVVMViewModelBase>* Found = CachedViewModels.Find(ContextName);
	return Found ? *Found : nullptr;
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
	
	const FArcCoreGameplayTags& InitTags = FArcCoreGameplayTags::Get();
	UGameFrameworkComponentManager* GFCM = UGameFrameworkComponentManager::GetForActor(GetOwner());
	
	if (GFCM)
	{
		RequestHandle = GFCM->AddExtensionHandler(GetOwner()->GetClass()
		, UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(this, &UArcFrontendMVVMModelComponent::HandleOnGameplayReady));
	}
}

void UArcFrontendMVVMModelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FInstancedStruct& Entry : StructModels)
	{
		FArcFrontendMVVMStructModel* StructModel = Entry.GetMutablePtr<FArcFrontendMVVMStructModel>();
		if (StructModel)
		{
			StructModel->Deinitialize(*this);
		}
	}

	RequestHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UArcFrontendMVVMModelComponent::HandleOnGameplayReady(AActor* Actor, FName EventName)
{
	UGameFrameworkComponentManager* GFCM = UGameFrameworkComponentManager::GetForActor(GetOwner());
	if (!GFCM)
	{
		return;
	}
	
	if (Actor != GetOwner())
	{
		return;
	}
	
	if (EventName != UArcHeroComponentBase::NAME_PlayerHeroPlayerStateInitialized)
	{
		return;
	}
	
	if (RequestHandle.IsValid())
	{
		RequestHandle.Reset();
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
	
	bIsDataReady = true;
	
	UArcPawnExtensionComponent* PawnExtensionComponent = GetOwner()->FindComponentByClass<UArcPawnExtensionComponent>();
	if (!PawnExtensionComponent)
	{
		if (APlayerState* PS = GetOwner<APlayerState>())
		{
			APawn* Pawn = PS->GetPawn();
			if (Pawn)
			{
				PawnExtensionComponent = Pawn->FindComponentByClass<UArcPawnExtensionComponent>();
			}
		}
		if (!PawnExtensionComponent)
		{
			if (APlayerController* PC = GetOwner<APlayerController>())
			{
				APawn* Pawn = PC->GetPawn();
				if (Pawn)
				{
					PawnExtensionComponent = Pawn->FindComponentByClass<UArcPawnExtensionComponent>();
				}
			}	
		}
	}
	
	if (PawnExtensionComponent)
	{
		//PawnExtensionComponent->CheckDefaultInitialization();
	}
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