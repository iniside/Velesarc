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
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "ArcReplicatedCommand.h"

#include "GameFramework/Character.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystem/ArcAttributesTypes.h"

#include "ArcDebugAddItemCommand.generated.h"

class ACharacter;

UCLASS()
class UArcTestAttributeSet : public UArcAttributeSet
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FGameplayAttributeData Health;

	UPROPERTY()
	FGameplayAttributeData MaxHealth;

	///
	/// Meta Attributes

	/// <summary>
	///
	/// </summary>
	UPROPERTY()
	FGameplayAttributeData Damage;

public:
	ARC_ATTRIBUTE_ACCESSORS(UArcTestAttributeSet
		, Health);
	ARC_ATTRIBUTE_ACCESSORS(UArcTestAttributeSet
		, MaxHealth);
	ARC_ATTRIBUTE_ACCESSORS(UArcTestAttributeSet
		, Damage);
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcDebugAddItemCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<ACharacter> Character = nullptr;

public:
	virtual bool CanSendCommand() const override;

	virtual void PreSendCommand() override;

	virtual bool Execute() override; ;

	FArcDebugAddItemCommand()
		: Character(nullptr)
	{
	}

	FArcDebugAddItemCommand(ACharacter* InCharacter)
		: Character(InCharacter)
	{
	}

	virtual ~FArcDebugAddItemCommand() override
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcDebugAddItemCommand::StaticStruct();
	}

	// so so you remember to override in child structs.
	// virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	// override;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcDebugModifyItemCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<ACharacter> Character = nullptr;

public:
	virtual bool CanSendCommand() const override;

	virtual void PreSendCommand() override;

	virtual bool Execute() override; ;

	FArcDebugModifyItemCommand()
		: Character(nullptr)
	{
	}

	FArcDebugModifyItemCommand(ACharacter* InCharacter)
		: Character(InCharacter)
	{
	}

	virtual ~FArcDebugModifyItemCommand() override
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcDebugModifyItemCommand::StaticStruct();
	}

	// so so you remember to override in child structs.
	// virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	// override;
};
