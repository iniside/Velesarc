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


#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "Items/ArcItemSpec.h"


#include "ArcItemTestsSettings.generated.h"

USTRUCT()
struct FArcCoreTestStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 Test = 0;
};

UCLASS()
class UArcCoreTestItemConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FArcItemSpec SpecConfig;

	UPROPERTY(EditAnywhere)
	FArcCoreTestStruct TestStruct;
	
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	
	virtual void Serialize(FArchive& Ar) override;
};

/**
 * 
 */
UCLASS(config = ArcTests, defaultconfig)
class ARCCORETEST_API UArcItemTestsSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemDefinitionId;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId SimpleBaseItem;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithGrantedAbility;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithDefaultAttachment;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithAttachmentSlots;
	
	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithEffectsToApply;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStatsA;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStatsB;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStatsAB;
	
	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStatsAC;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStatsC;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStatsBC;
	
	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithGrantedAbilityAndEffectsToApply;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithEffectToApply;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStacks;

	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStacksNoStackingPolicy;
	
	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStacksCannotStack;
	
	UPROPERTY(EditDefaultsOnly, Category = ItemTests, Config, meta = (AllowedClasses = "/Script/ArcCoreTest.ArcCoreTestItemConfig", DisplayThumbnail = false))
	FSoftObjectPath ItemConfig;
	
	/** Gets the settings container name for the settings, either Project or Editor */
	virtual FName GetContainerName() const override { return TEXT("Project"); };
	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const  override { return TEXT("Arc Tests"); };;
	/** The unique name for your section of settings, uses the class's FName. */
	virtual FName GetSectionName() const  override { return TEXT("Item Tests"); };;

#if WITH_EDITOR
	/** Gets the section text, uses the classes DisplayName by default. */
	virtual FText GetSectionText() const override { return FText::FromString("Item Tests"); };
	/** Gets the description for the section, uses the classes ToolTip by default. */
	virtual FText GetSectionDescription() const override { return FText::FromString("Item Tests"); };;
#endif
};
