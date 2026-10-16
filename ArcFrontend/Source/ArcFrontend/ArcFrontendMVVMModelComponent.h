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

#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "View/MVVMViewModelContextResolver.h"
#include "StructUtils/InstancedStruct.h"
#include "MVVMViewModelBase.h"
#include "Types/MVVMViewModelContext.h"
#include "ArcFrontendMVVMModelComponent.generated.h"

UCLASS(Blueprintable, EditInlineNew)
class ARCFRONTEND_API UArcFrontendMVVMComponentResolver : public UMVVMViewModelContextResolver
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn, Categories = "ViewModel"))
	FGameplayTag ViewModelTag;

public:
	virtual UObject* CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const override;
};

UCLASS(Blueprintable, EditInlineNew)
class ARCFRONTEND_API UArcFrontendMVVMWidgetSlotResolver : public UMVVMViewModelContextResolver
{
	GENERATED_BODY()

public:
	virtual UObject* CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const override;
};

class UArcFrontendMVVMModelComponent;

USTRUCT(BlueprintType)
struct ARCFRONTEND_API FArcViewModelSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName ContextName;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UMVVMViewModelBase> ViewModelClass;

	/** Optional. If valid, the created VM is also registered in UArcFrontendMVVMModelComponent::ViewModels under this tag, so tag-based resolvers can find it. */
	UPROPERTY(EditAnywhere, meta = (Categories = "ViewModel"))
	FGameplayTag RegistrationTag;
};

UCLASS(BlueprintType, Blueprintable, Within=ArcFrontendMVVMModelComponent)
class ARCFRONTEND_API UArcFrontendMVVMModel : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Ticking")
	bool bTickForAll = false;
	
	UPROPERTY(EditAnywhere, Category = "Ticking")
	bool bTickForLocalOnly = false;
	
	virtual void NaiveBeginPlay();

	void NativeTick(float DeltaTime);
	
	UFUNCTION(BlueprintImplementableEvent)
	void Tick(float DeltaTime);
	
	UFUNCTION(BlueprintImplementableEvent)
	void BeginPlay();

	UFUNCTION(BlueprintCallable, Category = "Arc Core")
	AActor* GetOwnerActor() const;

	UFUNCTION(BlueprintCallable, Category = "Arc Core")
	APlayerState* GetOwnerPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "Arc Core")
	APlayerController* GetOwnerPlayerController() const;
	
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "ComponentClass"))
	UActorComponent* GetComponentByClass(TSubclassOf<UActorComponent> ComponentClass) const;

	UFUNCTION(BlueprintCallable)
	UArcFrontendMVVMModelComponent* GetMVVMComponent() const;
	
	virtual UWorld* GetWorld() const override;

	virtual bool NeedsLoadForServer() const override { return false; };

};

USTRUCT(BlueprintType)
struct ARCFRONTEND_API FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ticking")
	bool bTickForAll = false;

	UPROPERTY(EditAnywhere, Category = "Ticking")
	bool bTickForLocalOnly = false;

	virtual ~FArcFrontendMVVMStructModel() = default;

	virtual void CreateViewModels(UArcFrontendMVVMModelComponent& Component);
	virtual void Initialize(UArcFrontendMVVMModelComponent& Component);
	virtual void Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component);
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component);
};

class UMVVMViewModelCollectionObject;
class UArcMassPlayerStateExtensionComponent;

UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent), DefaultToInstanced)
class ARCFRONTEND_API UArcFrontendMVVMModelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<UArcFrontendMVVMModel>> ModelClasses;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UArcFrontendMVVMModel>> ModelInstances;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UArcFrontendMVVMModel>> ModelTickLocal;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UArcFrontendMVVMModel>> ModelTickForAll;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "ViewModel"))
	TMap<FGameplayTag, TObjectPtr<UMVVMViewModelBase>> ViewModels;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcFrontend.ArcFrontendMVVMStructModel"))
	TArray<FInstancedStruct> StructModels;

	TArray<int32> StructTickLocalIndices;
	TArray<int32> StructTickForAllIndices;

	// Global-collection contexts this component successfully registered; removed on EndPlay.
	// Plain member (not a UPROPERTY): FMVVMViewModelContext holds a UClass pointer + FName,
	// mirroring the old non-UPROPERTY CachedViewModels.
	TArray<FMVVMViewModelContext> RegisteredGlobalContexts;

	UMVVMViewModelCollectionObject* GetGlobalViewModelCollection() const;

	FDelegateHandle InitStateHandle;

	UArcFrontendMVVMModelComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OnPlayerStateDataInitialized(UArcMassPlayerStateExtensionComponent* PSExt);

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	
	virtual bool NeedsLoadForServer() const override { return false; };
	
	UFUNCTION(BlueprintCallable, Category = "Arc Frontend", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool FindViewModel(UPARAM( meta = (Categories = "ViewModel")) FGameplayTag ViewModelTag, UMVVMViewModelBase*& ViewModel);

	UFUNCTION(BlueprintCallable, Category = "Arc Frontend")
	void AddViewModel(UPARAM( meta = (Categories = "ViewModel")) FGameplayTag ViewModelTag, UMVVMViewModelBase* InViewModel);

	UFUNCTION(BlueprintCallable, Category = "Arc Frontend")
	void RemoveViewModel(UPARAM( meta = (Categories = "ViewModel")) FGameplayTag ViewModelTag);

	/** Registers an existing VM into the global MVVM collection (by ContextName) and/or the component tag map (by RegistrationTag). Records the global context for EndPlay teardown only on a successful add. */
	void RegisterViewModel(UMVVMViewModelBase* VM, FName ContextName, FGameplayTag RegistrationTag);

	/** Creates a VM per spec and registers each via RegisterViewModel. */
	void CreateAndRegisterViewModels(TConstArrayView<FArcViewModelSpec> Specs);

	/** Resolves a VM handle from the global collection by exact ContextClass + ContextName. */
	UMVVMViewModelBase* FindViewModelByContext(FName ContextName, TSubclassOf<UMVVMViewModelBase> Class) const;
};
