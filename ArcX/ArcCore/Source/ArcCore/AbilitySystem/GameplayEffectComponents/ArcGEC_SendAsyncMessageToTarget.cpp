// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGEC_SendAsyncMessageToTarget.h"

#include "AbilitySystemComponent.h"
#include "AsyncGameplayMessageSystem.h"
#include "AsyncMessageBindingComponent.h"
#include "AsyncMessageWorldSubsystem.h"
#include "GameplayEffect.h"

void UArcGEC_SendAsyncMessageToTarget::OnGameplayEffectExecuted(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec
																, FPredictionKey& PredictionKey) const
{
	if (!ExecutedMessageTag.IsValid() || !ExecutedMessage.IsValid())
	{
		return;
	}
	
	UAbilitySystemComponent* ASC = ActiveGEContainer.Owner;
	if (!ASC)
	{
		return;
	}
	
	UWorld* World = ASC->GetWorld();
	if (!World)
	{
		return;
	}
	
	TSharedPtr<FAsyncGameplayMessageSystem> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(World);
	if (!MessageSystem.IsValid())
	{
		return;
	}
	
	IAsyncMessageBindingEndpointInterface* EndpointInterface = Cast<IAsyncMessageBindingEndpointInterface>(ASC->GetOwnerActor());
	if (!EndpointInterface)
	{
		UAsyncMessageBindingComponent* BindComp = ASC->GetOwnerActor()->FindComponentByClass<UAsyncMessageBindingComponent>();
		if (!BindComp)
		{
			return;
		}
	
		EndpointInterface = Cast<IAsyncMessageBindingEndpointInterface>(BindComp);	
	}
	
	if (!EndpointInterface)
	{
		return;
	}
	
	FAsyncMessageId MessageId(ExecutedMessageTag);
	
	MessageSystem->QueueMessageForBroadcast(MessageId, ExecutedMessage, EndpointInterface->GetEndpoint());
}

void UArcGEC_SendAsyncMessageToTarget::OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec
	, FPredictionKey& PredictionKey) const
{
	if (!AppliedMessageTag.IsValid() || !AppliedMessage.IsValid())
	{
		return;
	}
	
	UAbilitySystemComponent* ASC = ActiveGEContainer.Owner;
	if (!ASC)
	{
		return;
	}
	
	UWorld* World = ASC->GetWorld();
	if (!World)
	{
		return;
	}
	
	TSharedPtr<FAsyncGameplayMessageSystem> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(World);
	if (!MessageSystem.IsValid())
	{
		return;
	}
		
	IAsyncMessageBindingEndpointInterface* EndpointInterface = Cast<IAsyncMessageBindingEndpointInterface>(ASC->GetOwnerActor());
	if (!EndpointInterface)
	{
		UAsyncMessageBindingComponent* BindComp = ASC->GetOwnerActor()->FindComponentByClass<UAsyncMessageBindingComponent>();
		if (!BindComp)
		{
			return;
		}
	
		EndpointInterface = Cast<IAsyncMessageBindingEndpointInterface>(BindComp);	
	}
	
	if (!EndpointInterface)
	{
		return;
	}
	
	FAsyncMessageId MessageId(AppliedMessageTag);
	
	MessageSystem->QueueMessageForBroadcast(MessageId, AppliedMessage, EndpointInterface->GetEndpoint());
}
