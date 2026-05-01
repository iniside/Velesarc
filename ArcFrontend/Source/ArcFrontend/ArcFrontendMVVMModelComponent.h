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
#include "Components/GameFrameworkComponentDelegates.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Player/ArcHeroComponentBase.h"
#include "View/MVVMViewModelContextResolver.h"
#include "StructUtils/InstancedStruct.h"
#include "MVVMViewModelBase.h"
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

class UArcFrontendMVVMModelComponent;

USTRUCT(BlueprintType)
struct ARCFRONTEND_API FArcViewModelSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName ContextName;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UMVVMViewModelBase> ViewModelClass;
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

	UPROPERTY(EditAnywhere)
	TArray<FArcViewModelSpec> ViewModelSpecs;

	UPROPERTY(EditAnywhere, Category = "Ticking")
	bool bTickForAll = false;

	UPROPERTY(EditAnywhere, Category = "Ticking")
	bool bTickForLocalOnly = false;

	TMap<FName, TObjectPtr<UMVVMViewModelBase>> CachedViewModels;

	virtual ~FArcFrontendMVVMStructModel() = default;

	virtual void CreateViewModels(UArcFrontendMVVMModelComponent& Component);
	virtual void Initialize(UArcFrontendMVVMModelComponent& Component);
	virtual void Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component);
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component);

	UMVVMViewModelBase* FindCachedViewModel(FName ContextName) const;
};

class UMVVMViewModelCollectionObject;

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

	UMVVMViewModelCollectionObject* GetGlobalViewModelCollection() const;

	FDelegateHandle DelegateHandle;
	TSharedPtr<FComponentRequestHandle> RequestHandle;
	
	bool bIsDataReady = false;
	
	UArcFrontendMVVMModelComponent();
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	void HandleOnGameplayReady(AActor* Actor, FName EventName);
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	
	virtual bool NeedsLoadForServer() const override { return false; };
	
	UFUNCTION(BlueprintCallable, Category = "Arc Frontend", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool FindViewModel(UPARAM( meta = (Categories = "ViewModel")) FGameplayTag ViewModelTag, UMVVMViewModelBase*& ViewModel);

	UFUNCTION(BlueprintCallable, Category = "Arc Frontend")
	void AddViewModel(UPARAM( meta = (Categories = "ViewModel")) FGameplayTag ViewModelTag, UMVVMViewModelBase* InViewModel);
};
