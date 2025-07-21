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

#include "ArcSingleBotSpawner.h"

#include "AIController.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StateTreeComponent.h"

#include "Core/ArcCoreAssetManager.h"
#include "Misc/DataValidation.h"

void FArcBotPawnActorComponentFragment::GiveComponent(APawn* InPawn) const
{
	UActorComponent* Comp = NewObject<UActorComponent>(InPawn
		, Component
		, Component->GetFName());
	Comp->RegisterComponentWithWorld(InPawn->GetWorld());
}

void FArcBotRunBehaviorTree::GiveFragment(APawn* InPawn
										  , AAIController* InController) const
{
	InController->RunBehaviorTree(BehaviorTree);
}

void FArcBotRunStateTree::GiveFragment(class APawn* InPawn
	, class AAIController* InController) const
{
	UStateTreeAIComponent* Comp = NewObject<UStateTreeAIComponent>(InController
		, UStateTreeAIComponent::StaticClass()
		, UStateTreeAIComponent::StaticClass()->GetFName());
	
	Comp->SetStartLogicAutomatically(false);
	Comp->RegisterComponentWithWorld(InController->GetWorld());

	Comp->SetStateTree(StateTree);

	Comp->StartLogic();
}

void FArcBotGiveAttributeSet::GiveFragment(APawn* InPawn
										   , AAIController* InController) const
{
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InPawn);
	if (ASI == nullptr)
	{
		return;
	}

	UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());

	for (int32 SetIndex = 0; SetIndex < AttributeSets.Num(); ++SetIndex)
	{
		const FArcBotAttributeSet& SetToGrant = AttributeSets[SetIndex];

		if (!IsValid(SetToGrant.AttributeSetClass))
		{
			// UE_LOG(LogArcAbilitySet, Error, TEXT("GrantedAttributes[%d] on ability set
			// [%s] is not valid"), SetIndex, *GetNameSafe(this));
			continue;
		}

		UArcAttributeSet* NewSet = NewObject<UArcAttributeSet>(InPawn
			, SetToGrant.AttributeSetClass);
		ASC->AddAttributeSetSubobject(NewSet);
		ASC->InitStats(SetToGrant.AttributeSetClass
			, SetToGrant.AttributeMetaData.Get());
	}
	ASC->InitializeAttributeSets();
}

void FArcBotGiveAbilities::GiveFragment(APawn* InPawn
										, AAIController* InController) const
{
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InPawn);
	if (ASI == nullptr)
	{
		return;
	}

	UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());

	for (int32 AbilityIndex = 0; AbilityIndex < Abilities.Num(); ++AbilityIndex)
	{
		const TSubclassOf<class UGameplayAbility>& AbilityToGrant = Abilities[AbilityIndex];

		if (!IsValid(AbilityToGrant))
		{
			// UE_LOG(LogArcAbilitySet, Error, TEXT("GrantedGameplayAbilities [%d] on
			// ability set [%s] is not valid."), AbilityIndex, *GetNameSafe(this));
			continue;
		}

		UGameplayAbility* AbilityCDO = AbilityToGrant->GetDefaultObject<UGameplayAbility>();

		FGameplayAbilitySpec AbilitySpec(AbilityCDO
			, 0);
		AbilitySpec.SourceObject = InPawn;

		ASC->GiveAbility(AbilitySpec);
	}
}

void FArcBotGiveEffects::GiveFragment(class APawn* InPawn
	, class AAIController* InController) const
{
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InPawn);
	if (ASI == nullptr)
	{
		return;
	}

	UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	
	for (int32 AbilityIndex = 0; AbilityIndex < GameplayEffects.Num(); ++AbilityIndex)
	{
		const TSubclassOf<class UGameplayEffect>& AbilityToGrant = GameplayEffects[AbilityIndex];

		if (!IsValid(AbilityToGrant))
		{
			// UE_LOG(LogArcAbilitySet, Error, TEXT("GrantedGameplayAbilities [%d] on
			// ability set [%s] is not valid."), AbilityIndex, *GetNameSafe(this));
			continue;
		}

		ASC->ApplyGameplayEffectToSelf(AbilityToGrant->GetDefaultObject<UGameplayEffect>(), 1, ContextHandle);
	}
}

#if WITH_EDITOR
EDataValidationResult UArcBotData::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (PawnClass.IsNull() == true)
	{
		return Result;
	}

	const TSubclassOf<APawn> P = PawnClass.LoadSynchronous();

	if (P->GetDefaultObject<APawn>()->AutoPossessAI != EAutoPossessAI::Disabled)
	{
		Context.AddError(FText::FromString("AutoPossesAI should be set to EAutoPossessAI::Disabled"));
		return EDataValidationResult::Invalid;
	}

	return Result;
}
#endif // WITH_EDITOR

void UArcBotSpawnerLifetimeComponent::OnRegister()
{
	Super::OnRegister();
	GetOwner()->OnDestroyed.AddDynamic(this
		, &UArcBotSpawnerLifetimeComponent::HandleOnOwnerDestroyed);
}

void UArcBotSpawnerLifetimeComponent::HandleOnOwnerDestroyed(AActor* DestroyedActor)
{
	OnBotDestroyed.Broadcast();
	if (SpawnedBy.IsValid())
	{
		SpawnedBy->RespawnBot();
	}
}

// Sets default values
UArcSingleBotSpawner::UArcSingleBotSpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// Called when the game starts or when spawned
void UArcSingleBotSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority() == false)
	{
		return;
	}

	ENetMode NM = GetNetMode();
	if (NM == ENetMode::NM_Client)
	{
		return;
	}
	
	FSoftObjectPath Path = BotData.ToSoftObjectPath();

	UArcCoreAssetManager::Get().LoadAsset(Path
		, FStreamableDelegate::CreateUObject(this
			, &UArcSingleBotSpawner::OnBotDataLoaded));
}

void UArcSingleBotSpawner::OnBotDataLoaded()
{
	const UArcBotData* Data = BotData.Get();

	auto OnPawnClassLoaded = [this]()
	{
		const UArcBotData* Data = BotData.Get();

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.OverrideLevel = GetComponentLevel();
		SpawnInfo.ObjectFlags |= RF_Transient;

		AAIController* NewController = GetWorld()->SpawnActor<AAIController>(Data->ControllerClass
			, FVector::ZeroVector
			, FRotator::ZeroRotator
			, SpawnInfo);

		FTransform T;
		T.SetLocation(GetOwner()->GetActorLocation());
		T.SetRotation(FQuat::Identity);
		T.SetScale3D(FVector(1));

		APawn* NewPawn = GetWorld()->SpawnActorDeferred<APawn>(Data->PawnClass.Get()
			, T
			, nullptr
			, nullptr
			, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

		if (Data->DelayBeforeRespawn)
		{
			UArcBotSpawnerLifetimeComponent* Comp = NewObject<UArcBotSpawnerLifetimeComponent>(NewPawn
				, UArcBotSpawnerLifetimeComponent::StaticClass()
				, UArcBotSpawnerLifetimeComponent::StaticClass()->GetFName());
			Comp->RegisterComponentWithWorld(GetWorld());
			Comp->SpawnedBy = this;
		}

		for (const FInstancedStruct& IS : Data->PawnComponentsToGive)
		{
			if (const FArcBotPawnComponentFragment* BDF = IS.GetPtr<FArcBotPawnComponentFragment>())
			{
				BDF->GiveComponent(NewPawn);
			}
		}

		NewPawn->FinishSpawning(T);

		NewController->SetPawn(NewPawn);
		NewController->Possess(NewPawn);

		for (const FInstancedStruct& IS : Data->DataFragments)
		{
			if (const FArcBotDataFragment* BDF = IS.GetPtr<FArcBotDataFragment>())
			{
				BDF->GiveFragment(NewPawn
					, NewController);
			}
		}
	};

	FSoftObjectPath Path = Data->PawnClass.ToSoftObjectPath();

	UArcCoreAssetManager::Get().LoadAsset(Path
		, FStreamableDelegate::CreateLambda(OnPawnClassLoaded));
}

void UArcSingleBotSpawner::RespawnBot()
{
	UArcBotData* BD = BotData.Get();
	if (BD != nullptr)
	{
		if (BD->DelayBeforeRespawn > 0.f)
		{
			FTimerHandle Handle;
			if (GetWorld() != nullptr)
			{
				GetWorld()->GetTimerManager().SetTimer(Handle
					, FTimerDelegate::CreateUObject(this
						, &UArcSingleBotSpawner::OnBotDataLoaded)
					, BD->DelayBeforeRespawn
					, false);
			}
		}
		else
		{
			OnBotDataLoaded();
		}
	}
	else
	{
		auto OnLoaded = [this]()
		{
			UArcBotData* BD = BotData.Get();
			if (BD->DelayBeforeRespawn > 0.f)
			{
				FTimerHandle Handle;
				if (GetWorld() != nullptr)
				{
					GetWorld()->GetTimerManager().SetTimer(Handle
						, FTimerDelegate::CreateUObject(this
							, &UArcSingleBotSpawner::OnBotDataLoaded)
						, BD->DelayBeforeRespawn
						, false);
				}
			}
			else
			{
				OnBotDataLoaded();
			}
		};
		FSoftObjectPath Path = BotData.ToSoftObjectPath();
		UArcCoreAssetManager::Get().LoadAsset(Path
			, FStreamableDelegate::CreateLambda(OnLoaded));
	}
}

UArcAsyncSpawnSingleBot::UArcAsyncSpawnSingleBot()
{
}

UArcAsyncSpawnSingleBot* UArcAsyncSpawnSingleBot::SpawnSingleBot(UObject* WorldContextObject
																 , TSoftObjectPtr<UArcBotData> InBotData
																 , const FTransform& InTransform)
{
	UArcAsyncSpawnSingleBot* Action = nullptr;

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject
		, EGetWorldErrorMode::LogAndReturnNull))
	{
		Action = NewObject<UArcAsyncSpawnSingleBot>();
		Action->BotData = InBotData;
		Action->Transform = InTransform;
		Action->World = World;;
		Action->RegisterWithGameInstance(World);
	}

	return Action;
}

void UArcAsyncSpawnSingleBot::Activate()
{
	Super::Activate();

	FSoftObjectPath Path = BotData.ToSoftObjectPath();
	UArcCoreAssetManager::Get().LoadAsset(Path
		, FStreamableDelegate::CreateUObject(this
			, &UArcAsyncSpawnSingleBot::OnBotDataLoaded));
}

void UArcAsyncSpawnSingleBot::OnBotDataLoaded()
{
	auto OnPawnClassLoaded = [this]()
	{
		const UArcBotData* Data = BotData.Get();

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.OverrideLevel = World->PersistentLevel;
		SpawnInfo.ObjectFlags |= RF_Transient;

		AAIController* NewController = World->SpawnActor<AAIController>(Data->ControllerClass
			, FVector::ZeroVector
			, FRotator::ZeroRotator
			, SpawnInfo);

		FTransform T;
		T.SetLocation(Transform.GetLocation());
		T.SetRotation(FQuat::Identity);
		T.SetScale3D(FVector(1));

		APawn* NewPawn = World->SpawnActorDeferred<APawn>(Data->PawnClass.Get()
			, T
			, NewController
			, nullptr
			, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

		// if (Data->DelayBeforeRespawn)
		//{
		//	UArcBotSpawnerLifetimeComponent* Comp =
		// NewObject<UArcBotSpawnerLifetimeComponent>(NewPawn 		,
		// UArcBotSpawnerLifetimeComponent::StaticClass() 		,
		// UArcBotSpawnerLifetimeComponent::StaticClass()->GetFName());
		//	Comp->RegisterComponentWithWorld(GetWorld());
		//	Comp->SpawnedBy = this;
		// }

		for (const FInstancedStruct& IS : Data->PawnComponentsToGive)
		{
			if (const FArcBotPawnComponentFragment* BDF = IS.GetPtr<FArcBotPawnComponentFragment>())
			{
				BDF->GiveComponent(NewPawn);
			}
		}

		NewController->SetPawn(NewPawn);
		NewController->Possess(NewPawn);

		for (const FInstancedStruct& IS : Data->DataFragments)
		{
			if (const FArcBotDataFragment* BDF = IS.GetPtr<FArcBotDataFragment>())
			{
				BDF->GiveFragment(NewPawn
					, NewController);
			}
		}
		OnSpawned.Broadcast(NewPawn
			, NewController);

		NewPawn->FinishSpawning(T);
	};

	const UArcBotData* Data = BotData.Get();
	FSoftObjectPath Path = Data->PawnClass.ToSoftObjectPath();

	UArcCoreAssetManager::Get().LoadAsset(Path
		, FStreamableDelegate::CreateLambda(OnPawnClassLoaded));
}
