/**
 * This file is part of ArcX.
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



#include "ArcItemFragment_CustomizableObject.h"

#include "Equipment/ArcEquipmentUtils.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"

TArray<FString> UArcItemFragment_CustomizableObjectData::GetCustomizableParameters() const
{
	TArray<FString> CustomizableParameters;
	if (CustomizableObject.IsNull())
	{
		return CustomizableParameters;
	}

#if WITH_EDITOR
	FCompileParams Params;
	Params.bSkipIfCompiled = true;
	Params.bAsync = false;
	CustomizableObject.LoadSynchronous()->Compile(Params);
#endif
	
	int32 ParamNum = CustomizableObject->GetParameterCount();
	for (int32 i = 0; i < ParamNum; i++)
	{
		FString ParamName = CustomizableObject->GetParameterName(i);
		CustomizableParameters.AddUnique(ParamName);
	}

	return CustomizableParameters;
}

TArray<FString> UArcItemFragment_CustomizableObjectData::GetCustomizableParameterOptions() const
{
	TArray<FString> CustomizableParameters;
	if (CustomizableObject.IsNull())
	{
		return CustomizableParameters;
	}

	if (Parameters.IsEmpty())
	{
		return CustomizableParameters;
	}

#if WITH_EDITOR
	FCompileParams Params;
	Params.bSkipIfCompiled = true;
	Params.bAsync = false;
	CustomizableObject.LoadSynchronous()->Compile(Params);
#endif
	
	bool bHasParam = CustomizableObject->ContainsParameter(Parameters);
	if (!bHasParam)
	{
		return CustomizableParameters;	
	}
	
	int32 Num = CustomizableObject->GetEnumParameterNumValues(Parameters);
	for (int32 i = 0; i < Num; i++)
	{
		FString ParamName = CustomizableObject->GetEnumParameterValue(Parameters, i);
		CustomizableParameters.AddUnique(ParamName);
	}

	return CustomizableParameters;
}

void FArcAttachmentHandler_CustomizableObject::Initialize(UArcItemAttachmentComponent* InAttachmentComponent)
{
	AttachmentComponent = InAttachmentComponent;
	
	FCompileParams Params;
	Params.bSkipIfCompiled = true;
	Params.bAsync = true;
	Params.CallbackNative.BindRaw(this, &FArcAttachmentHandler_CustomizableObject::HandleOnCustomizableObjectCompiled);
	CustomizableObject.LoadSynchronous()->Compile(Params);
}

void FArcAttachmentHandler_CustomizableObject::HandleOnCustomizableObjectCompiled(const FCompileCallbackParams& Callback)
{
	bCustomizableObjectCompiled = true;
	for (const FArcItemFragment_CustomizableObject* Fragment : PendingFragments)
	{
		HandleCustomizableObjectItemFragment(Fragment);
	}
}

void FArcAttachmentHandler_CustomizableObject::HandleCustomizableObjectItemFragment(const FArcItemFragment_CustomizableObject* InItemFragment) const
{
	FString Parameter = InItemFragment->CustomizableObject->Parameters;
	FString Option = InItemFragment->CustomizableObject->ParameterOptions;

	APlayerState* PS = Cast<APlayerState>(AttachmentComponent->GetOwner());
	ACharacter* C = PS->GetPawn<ACharacter>();

	UCustomizableSkeletalComponent* CSC = C->FindComponentByClass<UCustomizableSkeletalComponent>();
	if (!CSC)
	{
		return;
	}
	
	if (!CSC->GetCustomizableObjectInstance())
	{
		CSC->SetCustomizableObjectInstance(CustomizableObject.LoadSynchronous()->CreateInstance());
	}
	
	CSC->GetCustomizableObjectInstance()->SetIntParameterSelectedOption(Parameter, Option);
	CSC->GetCustomizableObjectInstance()->UpdateSkeletalMeshAsync(true, true);
}

bool FArcAttachmentHandler_CustomizableObject::HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
																	 , const FArcItemData* InItem
																	 , const FArcItemData* InOwnerItem) const
{
	UArcItemDefinition* VisualItemDefinition = nullptr;
	
	// Items can be attached and/or grant attachment sockets.
	const FArcItemFragment_CustomizableObject* Fragment = ArcEquipmentUtils::GetAttachmentFragment<FArcItemFragment_CustomizableObject>(InItem, VisualItemDefinition);
	if (Fragment == nullptr && VisualItemDefinition == nullptr)
	{
		return false;
	}
	
	APlayerState* PS = Cast<APlayerState>(InAttachmentComponent->GetOwner());
	ACharacter* C = PS->GetPawn<ACharacter>();
	
	if (C == nullptr)
	{
		return false;
	}
	
	FArcItemAttachment Attached = MakeItemAttachment(InAttachmentComponent, InItem, InOwnerItem);
	
	InAttachmentComponent->AddAttachedItem(Attached);
	
	return true;
}

void FArcAttachmentHandler_CustomizableObject::HandleItemRemovedFromSlot(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemData* InItem
	, const FGameplayTag& SlotId
	, const FArcItemData* InOwnerItem) const
{
	
	InAttachmentComponent->RemoveAttachment(InItem->GetItemId());
}

void FArcAttachmentHandler_CustomizableObject::HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
														, const FArcItemId InItemId
														, const FArcItemId InOwnerItem) const
{
	const FArcItemAttachment* ItemAttachment = InAttachmentComponent->GetAttachment(InItemId);
	
	const FArcItemFragment_CustomizableObject* Fragment = ItemAttachment->ItemDefinition->FindFragment<FArcItemFragment_CustomizableObject>();
	if (!Fragment)
	{
		return;
	}

	if (!bCustomizableObjectCompiled)
	{
		PendingFragments.Add(Fragment);
		return;
	}
	
	HandleCustomizableObjectItemFragment(Fragment);

}

void FArcAttachmentHandler_CustomizableObject::HandleItemDetach(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemId InItemId
	, const FArcItemId InOwnerItem) const
{
	if (InAttachmentComponent->GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		const FArcItemAttachment* Attachment = InAttachmentComponent->GetAttachment(InItemId);
		if (Attachment == nullptr)
		{
			return;
		}

		const FArcItemFragment_CustomizableObject* Fragment = Attachment->ItemDefinition->FindFragment<FArcItemFragment_CustomizableObject>();
		if (!Fragment)
		{
			return;
		}
		
		FString Parameter = Fragment->CustomizableObject->Parameters;
		FString Option = Fragment->CustomizableObject->ParameterOptions;

		APlayerState* PS = Cast<APlayerState>(InAttachmentComponent->GetOwner());
		ACharacter* C = PS->GetPawn<ACharacter>();
		
		UCustomizableSkeletalComponent* CSC = C->FindComponentByClass<UCustomizableSkeletalComponent>();
		if (!CSC)
		{
			return;
		}
		
		CSC->GetCustomizableObjectInstance()->SetIntParameterSelectedOption(Parameter, "None");
		CSC->GetCustomizableObjectInstance()->UpdateSkeletalMeshAsync(true, true);
	}
}
