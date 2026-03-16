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

#include "CommonInputBaseTypes.h"
#include "EnhancedInputSubsystems.h"
#include "CustomSettings/LyraSettingKeyboardInput.h"
#include "DataSource/GameSettingDataSource.h"
#include "EditCondition/WhenCondition.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "GameSettingValueScalarDynamic.h"
#include "LyraGameSettingRegistry.h"
#include "LyraSettingsLocal.h"
#include "LyraSettingsShared.h"
#include "Player/ArcLocalPlayerBase.h"
#include "PlayerMappableInputConfig.h"

class ULocalPlayer;

#define LOCTEXT_NAMESPACE "Lyra"

UGameSettingCollection* ULyraGameSettingRegistry::InitializeMouseAndKeyboardSettings(UArcLocalPlayerBase* InLocalPlayer)
{
	UGameSettingCollection* Screen = NewObject<UGameSettingCollection>();
	Screen->SetDevName(TEXT("MouseAndKeyboardCollection"));
	Screen->SetDisplayName(LOCTEXT("MouseAndKeyboardCollection_Name", "Mouse & Keyboard"));
	Screen->Initialize(InLocalPlayer);

	const TSharedRef<FWhenCondition> WhenPlatformSupportsMouseAndKeyboard = MakeShared<FWhenCondition>(
		[](const ULocalPlayer*, FGameSettingEditableState& InOutEditState)
		{
			const UCommonInputPlatformSettings* PlatformInput = UPlatformSettingsManager::Get().GetSettingsForPlatform<UCommonInputPlatformSettings>();
			if (!PlatformInput->SupportsInputType(ECommonInputType::MouseAndKeyboard))
			{
				InOutEditState.Kill(TEXT("Platform does not support mouse and keyboard"));
			}
		});

	// Mouse Sensitivity
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* Sensitivity = NewObject<UGameSettingCollection>();
		Sensitivity->SetDevName(TEXT("MouseSensitivityCollection"));
		Sensitivity->SetDisplayName(LOCTEXT("MouseSensitivityCollection_Name", "Sensitivity"));
		Screen->AddSetting(Sensitivity);

		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("MouseSensitivityYaw"));
			Setting->SetDisplayName(LOCTEXT("MouseSensitivityYaw_Name", "X-Axis Sensitivity"));
			Setting->SetDescriptionRichText(LOCTEXT("MouseSensitivityYaw_Description", "Sets the sensitivity of the mouse's horizontal (x) axis. With higher settings the camera will move faster when looking left and right with the mouse."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(GetMouseSensitivityX));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(SetMouseSensitivityX));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetMouseSensitivityX());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::RawTwoDecimals);
			Setting->SetSourceRangeAndStep(TRange<double>(0, 10), 0.01);
			Setting->SetMinimumLimit(0.01);

			Setting->AddEditCondition(WhenPlatformSupportsMouseAndKeyboard);

			Sensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("MouseSensitivityPitch"));
			Setting->SetDisplayName(LOCTEXT("MouseSensitivityPitch_Name", "Y-Axis Sensitivity"));
			Setting->SetDescriptionRichText(LOCTEXT("MouseSensitivityPitch_Description", "Sets the sensitivity of the mouse's vertical (y) axis. With higher settings the camera will move faster when looking up and down with the mouse."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(GetMouseSensitivityY));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(SetMouseSensitivityY));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetMouseSensitivityY());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::RawTwoDecimals);
			Setting->SetSourceRangeAndStep(TRange<double>(0, 10), 0.01);
			Setting->SetMinimumLimit(0.01);

			Setting->AddEditCondition(WhenPlatformSupportsMouseAndKeyboard);

			Sensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("MouseTargetingMultiplier"));
			Setting->SetDisplayName(LOCTEXT("MouseTargetingMultiplier_Name", "Targeting Sensitivity"));
			Setting->SetDescriptionRichText(LOCTEXT("MouseTargetingMultiplier_Description", "Sets the modifier for reducing mouse sensitivity when targeting. 100% will have no slow down when targeting. Lower settings will have more slow down when targeting."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(GetTargetingMultiplier));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(SetTargetingMultiplier));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetTargetingMultiplier());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::RawTwoDecimals);
			Setting->SetSourceRangeAndStep(TRange<double>(0, 10), 0.01);
			Setting->SetMinimumLimit(0.01);

			Setting->AddEditCondition(WhenPlatformSupportsMouseAndKeyboard);

			Sensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("InvertVerticalAxis"));
			Setting->SetDisplayName(LOCTEXT("InvertVerticalAxis_Name", "Invert Vertical Axis"));
			Setting->SetDescriptionRichText(LOCTEXT("InvertVerticalAxis_Description", "Enable the inversion of the vertical look axis."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(GetInvertVerticalAxis));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(SetInvertVerticalAxis));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetInvertVerticalAxis());

			Setting->AddEditCondition(WhenPlatformSupportsMouseAndKeyboard);

			Sensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("InvertHorizontalAxis"));
			Setting->SetDisplayName(LOCTEXT("InvertHorizontalAxis_Name", "Invert Horizontal Axis"));
			Setting->SetDescriptionRichText(LOCTEXT("InvertHorizontalAxis_Description", "Enable the inversion of the Horizontal look axis."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(GetInvertHorizontalAxis));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH_LYRA(SetInvertHorizontalAxis));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetInvertHorizontalAxis());

			Setting->AddEditCondition(WhenPlatformSupportsMouseAndKeyboard);

			Sensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
	}

	// Bindings for Mouse & Keyboard - Automatically Generated
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* KeyBinding = NewObject<UGameSettingCollection>();
		KeyBinding->SetDevName(TEXT("KeyBindingCollection"));
		KeyBinding->SetDisplayName(LOCTEXT("KeyBindingCollection_Name", "Keyboard & Mouse"));
		Screen->AddSetting(KeyBinding);

		const UEnhancedInputLocalPlayerSubsystem* EISubsystem = InLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		const UEnhancedInputUserSettings* UserSettings = EISubsystem->GetUserSettings();

		// If you want to just get one profile pair, then you can do UserSettings->GetCurrentProfile

		// A map of key bindings mapped to their display category
		TMap<FString, UGameSettingCollection*> CategoryToSettingCollection;

		// Returns an existing setting collection for the display category if there is one.
		// If there isn't one, then it will create a new one and initialize it
		auto GetOrCreateSettingCollection = [&CategoryToSettingCollection, &Screen](FText DisplayCategory) -> UGameSettingCollection*
		{
			static const FString DefaultDevName = TEXT("Default_KBM");
			static const FText DefaultDevDisplayName = NSLOCTEXT("LyraInputSettings", "LyraInputDefaults", "Default Experiences");

			if (DisplayCategory.IsEmpty())
			{
				DisplayCategory = DefaultDevDisplayName;
			}
			
			FString DisplayCatString = DisplayCategory.ToString();
			
			if (UGameSettingCollection** ExistingCategory = CategoryToSettingCollection.Find(DisplayCatString))
			{
				return *ExistingCategory;
			}
			
			UGameSettingCollection* ConfigSettingCollection = NewObject<UGameSettingCollection>();
			ConfigSettingCollection->SetDevName(FName(DisplayCatString));
			ConfigSettingCollection->SetDisplayName(DisplayCategory);
			Screen->AddSetting(ConfigSettingCollection);
			CategoryToSettingCollection.Add(DisplayCatString, ConfigSettingCollection);
			
			return ConfigSettingCollection;
		};

		static TSet<FName> CreatedMappingNames;
		CreatedMappingNames.Reset();
		
		for (const TPair<FString, TObjectPtr<UEnhancedPlayerMappableKeyProfile>>& ProfilePair : UserSettings->GetAllAvailableKeyProfiles())
		{
			const FString& ProfileName = ProfilePair.Key;
			const TObjectPtr<UEnhancedPlayerMappableKeyProfile>& Profile = ProfilePair.Value;

			for (const TPair<FName, FKeyMappingRow>& RowPair : Profile->GetPlayerMappingRows())
			{
				// Create a setting row for anything with valid mappings and that we haven't created yet
				if (RowPair.Value.HasAnyMappings() /* && !CreatedMappingNames.Contains(RowPair.Key)*/)
				{
					// We only want keyboard keys on this settings screen, so we will filter down by mappings
					// that are set to keyboard keys
					FPlayerMappableKeyQueryOptions Options = {};
					Options.KeyToMatch = EKeys::W;
					Options.bMatchBasicKeyTypes = true;
															
					const FText& DesiredDisplayCategory = RowPair.Value.Mappings.begin()->GetDisplayCategory();
					
					if (UGameSettingCollection* Collection = GetOrCreateSettingCollection(DesiredDisplayCategory))
					{
						// Create the settings widget and initialize it, adding it to this config's section
						ULyraSettingKeyboardInput* InputBinding = NewObject<ULyraSettingKeyboardInput>();

						InputBinding->InitializeInputData(Profile, RowPair.Value, Options);
						InputBinding->AddEditCondition(WhenPlatformSupportsMouseAndKeyboard);

						Collection->AddSetting(InputBinding);
						CreatedMappingNames.Add(RowPair.Key);
					}
					else
					{
						ensure(false);
					}
				}
			}
		}
	}

	return Screen;
}

#undef LOCTEXT_NAMESPACE