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



#pragma once

#include "ArcItemFragment_ItemAttachment.h"
#include "Equipment/ArcAttachmentHandler.h"
#include "ArcItemFragment_CustomizableObject.generated.h"

class UCustomizableObjectInstance;
class UCustomizableObject;
class UArcItemAttachmentComponent;
struct FCompileCallbackParams;

UCLASS(EditInlineNew, DefaultToInstanced)
class ARCCORE_API UArcItemFragment_CustomizableObjectData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomizableObject", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UCustomizableObject> CustomizableObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomizableObject", meta = (GetOptions = GetCustomizableParameters))
	FString Parameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomizableObject", meta = (GetOptions = GetCustomizableParameterOptions))
	FString ParameterOptions;
	
	UFUNCTION(BlueprintCallable, Category = "CustomizableObject")
	TArray<FString> GetCustomizableParameters() const;

	UFUNCTION(BlueprintCallable, Category = "CustomizableObject")
	TArray<FString> GetCustomizableParameterOptions() const;
};
/**
 * 
 */
USTRUCT()
struct ARCCORE_API FArcItemFragment_CustomizableObject : public FArcItemFragment_ItemAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Instanced, meta = (AssetBundles = "Client"))
	TObjectPtr<UArcItemFragment_CustomizableObjectData> CustomizableObject;
	
	UPROPERTY(EditAnywhere, Category = "CustomizableObject", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UCustomizableObject> CustomizableObjectSoft;
};

USTRUCT()
struct ARCCORE_API FArcAttachmentHandler_CustomizableObject : public FArcAttachmentHandlerCommon
{
	GENERATED_BODY()

protected:
	//UPROPERTY(EditAnywhere, Category = "Data", meta = (NoCategoryGrouping))
	//TSubclassOf<UChaosClothComponent> ComponentClass;

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UCustomizableObject> CustomizableObject;

	bool bCustomizableObjectCompiled = false;

	mutable TWeakObjectPtr<UArcItemAttachmentComponent> AttachmentComponent;
	
	mutable TArray<const FArcItemFragment_CustomizableObject*> PendingFragments;
	
public:
	virtual void Initialize(UArcItemAttachmentComponent* InAttachmentComponent) override;
	void HandleOnCustomizableObjectCompiled(const FCompileCallbackParams& Callback) ;
	void HandleCustomizableObjectItemFragment(const FArcItemFragment_CustomizableObject* InItemFragment) const;
	
	virtual bool HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
									   , const FArcItemData* InItem
									   , const FArcItemData* InOwnerItem) const override;

	virtual void HandleItemRemovedFromSlot(UArcItemAttachmentComponent* InAttachmentComponent
										   , const FArcItemData* InItem
										   , const FGameplayTag& SlotId
										   , const FArcItemData* InOwnerItem) const override;;

	virtual void HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
							  , const FArcItemId InItemId
							  , const FArcItemId InOwnerItem) const override;

	virtual void HandleItemDetach(UArcItemAttachmentComponent* InAttachmentComponent
							  , const FArcItemId InItemId
							  , const FArcItemId InOwnerItem) const;

	virtual UScriptStruct* SupportedItemFragment() const override
	{
		return FArcItemFragment_CustomizableObject::StaticStruct();
	}
	
	virtual ~FArcAttachmentHandler_CustomizableObject() override = default;
};
