// Copyright Lukasz Baran. All Rights Reserved.

#include "Host/ArcRmlUISettingsSubsystem.h"

#include "ArcRmlUITags.h"
#include "Descriptors/ArcSettingBuilder.h"
#include "Descriptors/ArcSettingDescriptor.h"
#include "Engine/GameInstance.h"
#include "Engine/UserInterfaceSettings.h"
#include "Model/ArcSettingsModel.h"

#define LOCTEXT_NAMESPACE "ArcRmlUISettings"

namespace ArcRmlUISettingsSubsystem_Internal
{
	// UUserInterfaceSettings is a process-wide CDO, so several PIE game instances share one restore point.
	int32 ApplicationScaleOwners = 0;
	float ApplicationScaleBeforeOwners = 1.0f;

	constexpr float DefaultUIScale = 1.0f;
	const TCHAR* const MaxUIAspectRatioValues[] = { TEXT("1.777778"), TEXT("2.333333"), TEXT("3.555556"), TEXT("0") };
	constexpr int32 DefaultMaxUIAspectRatioIndex = 0;

	FArcSettingOption MakeOption(const TCHAR* Value, FText DisplayText)
	{
		FArcSettingOption Option;
		Option.Value = Value;
		Option.DisplayText = MoveTemp(DisplayText);
		return Option;
	}
}

FArcRmlUISettingsDefaults UArcRmlUISettingsSubsystem::GetDefaults()
{
	FArcRmlUISettingsDefaults Defaults;
	Defaults.UIScale = ArcRmlUISettingsSubsystem_Internal::DefaultUIScale;
	Defaults.MaxUIAspectRatio = FCString::Atof(ArcRmlUISettingsSubsystem_Internal::MaxUIAspectRatioValues[ArcRmlUISettingsSubsystem_Internal::DefaultMaxUIAspectRatioIndex]);
	return Defaults;
}

bool UArcRmlUISettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UGameInstance* GameInstance = Cast<UGameInstance>(Outer);
	return GameInstance != nullptr && !GameInstance->IsDedicatedServerInstance();
}

void UArcRmlUISettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UArcSettingsModel* Model = Collection.InitializeDependency<UArcSettingsModel>();
	if (Model == nullptr)
	{
		return;
	}

	if (ArcRmlUISettingsSubsystem_Internal::ApplicationScaleOwners == 0)
	{
		ArcRmlUISettingsSubsystem_Internal::ApplicationScaleBeforeOwners = GetDefault<UUserInterfaceSettings>()->ApplicationScale;
	}
	++ArcRmlUISettingsSubsystem_Internal::ApplicationScaleOwners;
	bOwnsApplicationScale = true;

	RegisterSettings(*Model);
	ApplyUIScale(*Model);
}

void UArcRmlUISettingsSubsystem::Deinitialize()
{
	if (UArcSettingsModel* Model = GetGameInstance()->GetSubsystem<UArcSettingsModel>())
	{
		Model->UnregisterOnChanged(ArcRmlUITags::Settings_Video_Scalar_UIScale);
	}

	if (bOwnsApplicationScale)
	{
		bOwnsApplicationScale = false;
		--ArcRmlUISettingsSubsystem_Internal::ApplicationScaleOwners;
		if (ArcRmlUISettingsSubsystem_Internal::ApplicationScaleOwners == 0)
		{
			GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale = ArcRmlUISettingsSubsystem_Internal::ApplicationScaleBeforeOwners;
		}
	}

	Super::Deinitialize();
}

void UArcRmlUISettingsSubsystem::RegisterSettings(UArcSettingsModel& Model)
{
	using namespace ArcRmlUISettingsSubsystem_Internal;

	Model.Scalar(ArcRmlUITags::Settings_Video_Scalar_UIScale, ArcRmlUITags::Settings_Video)
		.Name(LOCTEXT("UIScale", "UI Scale"))
		.Local(TEXT("UIScale"))
		.Range(0.5, 2.0, 0.05)
		.Default(DefaultUIScale)
		.Format(EArcSettingDisplayFormat::Percent)
		.Done();

	Model.Discrete(ArcRmlUITags::Settings_Video_Discrete_MaxUIAspectRatio, ArcRmlUITags::Settings_Video)
		.Name(LOCTEXT("MaxUIAspectRatio", "Max UI Aspect Ratio"))
		.Local(TEXT("MaxUIAspectRatio"))
		.Options({
			MakeOption(MaxUIAspectRatioValues[0], LOCTEXT("MaxUIAspectRatio_16x9", "16:9")),
			MakeOption(MaxUIAspectRatioValues[1], LOCTEXT("MaxUIAspectRatio_21x9", "21:9")),
			MakeOption(MaxUIAspectRatioValues[2], LOCTEXT("MaxUIAspectRatio_32x9", "32:9")),
			MakeOption(MaxUIAspectRatioValues[3], LOCTEXT("MaxUIAspectRatio_NoLimit", "No limit"))
		})
		.Default(DefaultMaxUIAspectRatioIndex)
		.Done();

	Model.RegisterOnChanged(ArcRmlUITags::Settings_Video_Scalar_UIScale,
		[](UArcSettingsModel& InModel, FGameplayTag)
		{
			ApplyUIScale(InModel);
		});
}

void UArcRmlUISettingsSubsystem::ApplyUIScale(const UArcSettingsModel& Model)
{
	GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale = static_cast<float>(Model.GetScalarValue(ArcRmlUITags::Settings_Video_Scalar_UIScale));
}

#undef LOCTEXT_NAMESPACE
