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
#include "AbilitySystem/ArcAbilitySet.h"
#include "ArcCoreEditorModule.h"
#include "CoreMinimal.h"

#include "AssetTypeActions/AssetTypeActions_Blueprint.h"

#include "GameMode/ArcExperienceData.h"
#include "GameMode/ArcExperienceDefinition.h"
#include "Input/ArcInputActionConfig.h"
#include "Pawn/ArcPawnData.h"

#include "Factories/BlueprintFactory.h"
#include "Factories/Factory.h"

#include "QuickBar/ArcQuickBarComponent.h"
#include "AssetDefinitionDefault.h"
#include "Items/ArcItemsStoreComponent.h"

#include "ArcAssetTypeAction_InputPlayerSettings.generated.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_ArcCore"

class FArcAssetTypeAction_InputActionConfig : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override
	{
		return FText::FromString("Arc Input Action Config");
	}

	virtual FColor GetTypeColor() const override
	{
		return FColor(255
			, 227
			, 158);
	}

	virtual UClass* GetSupportedClass() const override
	{
		return UArcInputActionConfig::StaticClass();
	}

	virtual uint32 GetCategories() override
	{
		return FArcCoreEditorModule::GetArcAssetCategory() | FArcCoreEditorModule::GetInputAssetCategory();
	}
};

UCLASS()
class UArcAssetDefinition_InputActionConfig : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Implementation
	virtual FText GetAssetDisplayName() const override
	{
		return NSLOCTEXT("AssetDefinition"
			, "ArcInputActionConfig"
			, "Arc Input Action Config");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(255
			, 227
			, 158));
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath AssetPaths[] = {
			FAssetCategoryPath(LOCTEXT("Input", "Input"))
			, FAssetCategoryPath(LOCTEXT("ArcCategory", "Arc Core")
				, LOCTEXT("ArcExperienceCategory", "Input"))
		};

		return AssetPaths;
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcInputActionConfig::StaticClass();
	}
};

UCLASS()
class UArcInputActionConfig_Factory : public UFactory
{
	GENERATED_BODY()

public:
	UArcInputActionConfig_Factory();

	virtual UObject* FactoryCreateNew(UClass* Class
									  , UObject* InParent
									  , FName Name
									  , EObjectFlags Flags
									  , UObject* Context
									  , FFeedbackContext* Warn) override;
};

UCLASS()
class UArcAssetDefinition_PawnData : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Implementation
	virtual FText GetAssetDisplayName() const override
	{
		return NSLOCTEXT("AssetDefinition"
			, "PawnData"
			, "Arc Pawn Data");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(219
			, 255
			, 29));
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath AssetPaths[] = {
			FAssetCategoryPath(LOCTEXT("ArcCategory", "Arc Core")
				, LOCTEXT("ArcExperienceCategory", "Experience Data"))
		};

		return AssetPaths;
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcPawnData::StaticClass();
	}

	virtual EAssetCommandResult PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const override;
};

UCLASS()
class UArcPawnData_Factory : public UFactory
{
	GENERATED_BODY()

public:
	UArcPawnData_Factory();

	virtual UObject* FactoryCreateNew(UClass* Class
									  , UObject* InParent
									  , FName Name
									  , EObjectFlags Flags
									  , UObject* Context
									  , FFeedbackContext* Warn) override;
};

UCLASS()
class UArcAssetDefinition_ExperienceActionSet : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Implementation
	virtual FText GetAssetDisplayName() const override
	{
		return NSLOCTEXT("AssetDefinition"
			, "ExperienceActionSet"
			, "Experience Action Set");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(221, 97, 25));
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath AssetPaths[] = {
			FAssetCategoryPath(LOCTEXT("ArcCategory", "Arc Core")
				, LOCTEXT("ArcExperienceCategory", "Experience Data"))
		};

		return AssetPaths;
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcExperienceActionSet::StaticClass();
	}
};

UCLASS()
class UArcExperienceActionSet_Factory : public UFactory
{
	GENERATED_BODY()

public:
	UArcExperienceActionSet_Factory();

	virtual UObject* FactoryCreateNew(UClass* Class
									  , UObject* InParent
									  , FName Name
									  , EObjectFlags Flags
									  , UObject* Context
									  , FFeedbackContext* Warn) override;
};


UCLASS()
class UArcAssetDefinition_ExperienceDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Implementation
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("ArcExperienceDefinition"
			, "Arc Experience Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(219
			, 255
			, 29));
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath AssetPaths[] = {
			FAssetCategoryPath(LOCTEXT("ArcCategory"
					, "Arc Core")
				, LOCTEXT("ArcExperienceCategory"
					, "Experience Data"))
		};

		return AssetPaths;
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcExperienceDefinition::StaticClass();
	}
};

UCLASS()
class UArcExperienceDefinition_Factory : public UFactory
{
	GENERATED_BODY()

public:
	UArcExperienceDefinition_Factory();

	virtual UObject* FactoryCreateNew(UClass* Class
									  , UObject* InParent
									  , FName Name
									  , EObjectFlags Flags
									  , UObject* Context
									  , FFeedbackContext* Warn) override;
};

UCLASS()
class UArcAssetDefinition_ExperienceData : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Implementation
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("ArcExperienceData"
			, "Arc Experience Data");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(219
			, 255
			, 29));
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath AssetPaths[] = {
			FAssetCategoryPath(LOCTEXT("ArcCategory"
					, "Arc Core")
				, LOCTEXT("ArcExperienceCategory"
					, "Experience Data"))
		};

		return AssetPaths;
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcExperienceData::StaticClass();
	}
};

UCLASS()
class UArcExperienceData_Factory : public UFactory
{
	GENERATED_BODY()

public:
	UArcExperienceData_Factory();

	virtual UObject* FactoryCreateNew(UClass* Class
									  , UObject* InParent
									  , FName Name
									  , EObjectFlags Flags
									  , UObject* Context
									  , FFeedbackContext* Warn) override;
};

UCLASS()
class UArcAssetDefinition_AbilitySet : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Implementation
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("ArcAbilitySet"
			, "Arc Ability Set");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(219
			, 255
			, 29));
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath AssetPaths[] = {
			FAssetCategoryPath(LOCTEXT("ArcCategory"
					, "Arc Core")
				, LOCTEXT("ArcExperienceCategory"
					, "Experience Data"))
		};

		return AssetPaths;
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcAbilitySet::StaticClass();
	}
};

UCLASS()
class UArcAbilitySet_Factory : public UFactory
{
	GENERATED_BODY()

public:
	UArcAbilitySet_Factory();

	virtual UObject* FactoryCreateNew(UClass* Class
									  , UObject* InParent
									  , FName Name
									  , EObjectFlags Flags
									  , UObject* Context
									  , FFeedbackContext* Warn) override;
};

UCLASS()
class UArcComponentFactory : public UBlueprintFactory
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSubclassOf<UBlueprint> BlueprintClassType;

	UPROPERTY(EditAnywhere
		, Category = BlueprintFactory
		, meta = (AllowAbstract = "", BlueprintBaseOnly = ""))
	TSubclassOf<class UObject> BaseClass;

	UArcComponentFactory();

	virtual bool CanCreateNew() const override
	{
		return false;
	};

	virtual bool ConfigureProperties() override;

	virtual UObject* FactoryCreateNew(UClass* Class
									  , UObject* InParent
									  , FName Name
									  , EObjectFlags Flags
									  , UObject* Context
									  , FFeedbackContext* Warn
									  , FName CallingContext) override;
};

UCLASS()
class UArcQuickBarComponent_Factory : public UArcComponentFactory
{
	GENERATED_BODY()

public:
	UArcQuickBarComponent_Factory();

	virtual bool CanCreateNew() const override
	{
		return true;
	};
};

class FArcAssetTypeActions_QuickBarComponent : public FAssetTypeActions_Blueprint
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override
	{
		return FText::FromString("Arc Quick Bar Component");
	};

	virtual FColor GetTypeColor() const override
	{
		return FColor(61
			, 99
			, 255);
	};

	virtual UClass* GetSupportedClass() const override
	{
		return UArcQuickBarComponentBlueprint::StaticClass();
	};

	virtual uint32 GetCategories() override
	{
		return FArcCoreEditorModule::GetArcAssetCategory();
	}

	// End IAssetTypeActions Implementation

	// FAssetTypeActions_Blueprint interface
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;
};

UCLASS()
class UArcItemsStoreComponent_Factory : public UArcComponentFactory
{
	GENERATED_BODY()

public:
	UArcItemsStoreComponent_Factory();

	virtual bool CanCreateNew() const override
	{
		return true;
	};
};

class FArcAssetTypeActions_ItemsStoreComponent : public FAssetTypeActions_Blueprint
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override
	{
		return FText::FromString("Arc Item Store Component");
	};

	virtual FColor GetTypeColor() const override
	{
		return FColor(61
			, 99
			, 255);
	};

	virtual UClass* GetSupportedClass() const override
	{
		return UArcItemsStoreComponentBlueprint::StaticClass();
	};

	virtual uint32 GetCategories() override
	{
		return FArcCoreEditorModule::GetArcAssetCategory();
	}

	// End IAssetTypeActions Implementation

	// FAssetTypeActions_Blueprint interface
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;
};

#undef LOCTEXT_NAMESPACE
