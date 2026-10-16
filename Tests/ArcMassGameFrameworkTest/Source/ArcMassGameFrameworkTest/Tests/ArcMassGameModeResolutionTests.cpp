// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CommandLine.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

#include "ArcMassGameFrameworkTestSettings.h"
#include "Experience/ArcMassExperienceDefinition.h"
#include "GameMode/ArcMassGameMode.h"
#include "GameMode/ArcMassWorldSettings.h"

namespace ArcMassFrameworkTests::GameModeResolution
{
	/**
	 * AArcMassGameMode::GetExperiencePrimaryId is protected. We can't subclass with a
	 * UCLASS in this .cpp without UHT plumbing, but the runtime resolver only depends
	 * on (a) AGameModeBase::OptionsString (public), (b) FCommandLine, and (c) the
	 * world's AArcMassWorldSettings cast. We invoke the protected method via a
	 * pointer-to-member-function captured in a friend-style helper using the address
	 * of a local override... but the simplest solution is to mirror the production
	 * code path here. Since the resolver is short and stable, instead expose it via
	 * a thin reflection-driven wrapper that calls into the live resolver through a
	 * UFUNCTION-discoverable path is overkill. We use a cast trick: derive a non-
	 * UCLASS local accessor that promotes the protected member to public via a using-
	 * declaration. This is well-defined: the address of the function is the same as
	 * on the parent.
	 */
	/**
	 * Take a pointer-to-member-function to AArcMassGameMode::GetExperiencePrimaryId
	 * via a derived-class scope: a using-declaration in a friend-style derived class
	 * makes the address-of expression access-legal, while the resulting pointer is
	 * type-compatible with the parent and can be invoked on a parent instance --
	 * pointer-to-member-functions are not type-narrowed by access specifiers, only
	 * the address-of expression is access-checked.
	 */
	struct FGameModeAccessor : public AArcMassGameMode
	{
		using AArcMassGameMode::GetExperiencePrimaryId;
		using TResolverPtr = FPrimaryAssetId (AArcMassGameMode::*)(FString&) const;
		static TResolverPtr GetResolverPtr()
		{
			return static_cast<TResolverPtr>(&FGameModeAccessor::GetExperiencePrimaryId);
		}
	};

	static FPrimaryAssetId CallResolver(AArcMassGameMode& GameMode, FString& OutSource)
	{
		const FGameModeAccessor::TResolverPtr Fn = FGameModeAccessor::GetResolverPtr();
		return (GameMode.*Fn)(OutSource);
	}

	static FSoftObjectPath GetFrontendExperiencePath()
	{
		return FSoftObjectPath(TEXT("/Game/ArcGame/Maps/MassFrontendExperience.MassFrontendExperience"));
	}

	// Loads the experience and requires the AssetManager to know it, so the resolver can find it by name.
	static UArcMassExperienceDefinition* LoadRegisteredExperience(FAutomationTestBase& Test,
		const FSoftObjectPath& ObjectPath)
	{
		UArcMassExperienceDefinition* Experience =
			Cast<UArcMassExperienceDefinition>(ObjectPath.TryLoad());
		if (Experience == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("Failed to load experience asset %s"), *ObjectPath.ToString()));
			return nullptr;
		}
		if (!UAssetManager::Get().GetPrimaryAssetPath(Experience->GetPrimaryAssetId()).IsValid())
		{
			Test.AddError(FString::Printf(TEXT("Experience %s is not registered with the AssetManager"),
				*ObjectPath.ToString()));
			return nullptr;
		}
		return Experience;
	}

	static FSoftObjectPath GetBundleExperiencePath()
	{
		return GetDefault<UArcMassGameFrameworkTestSettings>()->TestExperience.ToSoftObjectPath();
	}

	static bool SetWorldSettingsDefaultExperience(AArcMassWorldSettings& WorldSettings,
		const TSoftObjectPtr<UArcMassExperienceDefinition>& Soft)
	{
		FProperty* Property = AArcMassWorldSettings::StaticClass()->FindPropertyByName(
			TEXT("DefaultExperience"));
		if (Property == nullptr)
		{
			return false;
		}
		void* ValueRaw = reinterpret_cast<uint8*>(&WorldSettings) + Property->GetOffset_ForInternal();
		TSoftObjectPtr<UArcMassExperienceDefinition>* TypedPtr =
			reinterpret_cast<TSoftObjectPtr<UArcMassExperienceDefinition>*>(ValueRaw);
		*TypedPtr = Soft;
		return true;
	}
}

/**
 * Verifies AArcMassGameMode::GetExperiencePrimaryId resolves the experience id from the
 * three legal sources in priority order:
 *   1) URL options (?Experience=...)
 *   2) Command line (-Experience=...)
 *   3) World settings (AArcMassWorldSettings::DefaultExperience)
 *
 * Command-line restore strategy: snapshot FCommandLine::Get() in BEFORE_EACH and restore
 * via FCommandLine::Set() in AFTER_EACH. FCommandLine::Set is part of the public API
 * and unconditional (verified in Engine/Source/Runtime/Core/Public/Misc/CommandLine.h).
 *
 * The test world's WorldSettingsClass is wired to AArcMassWorldSettings via DefaultEngine.ini
 * (WorldSettingsClassName=/Script/ArcMassGameplayFramework.ArcMassWorldSettings), so
 * GetWorldSettings() on the test world returns the typed actor directly -- no SetWorldSettings
 * gymnastics required.
 */
TEST_CLASS(ArcMassGameModeResolution_Tests, "ArcMassGameFramework.GameMode.ExperienceResolution")
{
	FActorTestSpawner Spawner;
	FString OriginalCommandLine;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		OriginalCommandLine = FString(FCommandLine::Get());
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	AFTER_EACH()
	{
		// FCommandLine::Set replaces the entire command line, providing a clean inverse
		// for any FCommandLine::Append calls a test made.
		FCommandLine::Set(*OriginalCommandLine);
	}

	TEST_METHOD(Resolution_URL_Wins)
	{
		AArcMassGameMode& GameMode = Spawner.SpawnActor<AArcMassGameMode>();

		UArcMassExperienceDefinition* UrlExperience = ArcMassFrameworkTests::GameModeResolution::
			LoadRegisteredExperience(*TestRunner, ArcMassFrameworkTests::GameModeResolution::GetFrontendExperiencePath());
		UArcMassExperienceDefinition* CmdLineExperience = ArcMassFrameworkTests::GameModeResolution::
			LoadRegisteredExperience(*TestRunner, ArcMassFrameworkTests::GameModeResolution::GetBundleExperiencePath());
		ASSERT_THAT(IsNotNull(UrlExperience));
		ASSERT_THAT(IsNotNull(CmdLineExperience));
		ASSERT_THAT(IsTrue(UrlExperience->GetPrimaryAssetId() != CmdLineExperience->GetPrimaryAssetId()));

		GameMode.OptionsString = FString::Printf(TEXT("?Experience=%s"), *UrlExperience->GetName());
		FCommandLine::Append(*FString::Printf(TEXT(" -Experience=%s"), *CmdLineExperience->GetName()));

		FString Source;
		const FPrimaryAssetId Id =
			ArcMassFrameworkTests::GameModeResolution::CallResolver(GameMode, Source);

		ASSERT_THAT(IsTrue(Id.IsValid()));
		ASSERT_THAT(AreEqual(FString(TEXT("OptionsString")), Source));
		ASSERT_THAT(IsTrue(Id == UrlExperience->GetPrimaryAssetId()));
	}

	TEST_METHOD(Resolution_CmdLine_BeatsWorldSettings)
	{
		AArcMassGameMode& GameMode = Spawner.SpawnActor<AArcMassGameMode>();

		UArcMassExperienceDefinition* CmdLineExperience = ArcMassFrameworkTests::GameModeResolution::
			LoadRegisteredExperience(*TestRunner, ArcMassFrameworkTests::GameModeResolution::GetBundleExperiencePath());
		UArcMassExperienceDefinition* WorldSettingsExperience = ArcMassFrameworkTests::GameModeResolution::
			LoadRegisteredExperience(*TestRunner, ArcMassFrameworkTests::GameModeResolution::GetFrontendExperiencePath());
		ASSERT_THAT(IsNotNull(CmdLineExperience));
		ASSERT_THAT(IsNotNull(WorldSettingsExperience));
		ASSERT_THAT(IsTrue(CmdLineExperience->GetPrimaryAssetId() != WorldSettingsExperience->GetPrimaryAssetId()));

		GameMode.OptionsString.Reset();
		FCommandLine::Append(*FString::Printf(TEXT(" -Experience=%s"), *CmdLineExperience->GetName()));

		AArcMassWorldSettings* TypedWorldSettings =
			Cast<AArcMassWorldSettings>(Spawner.GetWorld().GetWorldSettings());
		ASSERT_THAT(IsNotNull(TypedWorldSettings));

		const bool bSet = ArcMassFrameworkTests::GameModeResolution::SetWorldSettingsDefaultExperience(
			*TypedWorldSettings, TSoftObjectPtr<UArcMassExperienceDefinition>(WorldSettingsExperience));
		ASSERT_THAT(IsTrue(bSet));

		FString Source;
		const FPrimaryAssetId Id =
			ArcMassFrameworkTests::GameModeResolution::CallResolver(GameMode, Source);

		ASSERT_THAT(IsTrue(Id.IsValid()));
		ASSERT_THAT(AreEqual(FString(TEXT("CommandLine")), Source));
		ASSERT_THAT(IsTrue(Id == CmdLineExperience->GetPrimaryAssetId()));
	}

	TEST_METHOD(Resolution_UnknownName_ReturnsInvalid)
	{
		AArcMassGameMode& GameMode = Spawner.SpawnActor<AArcMassGameMode>();

		UArcMassExperienceDefinition* FallbackExperience = ArcMassFrameworkTests::GameModeResolution::
			LoadRegisteredExperience(*TestRunner, ArcMassFrameworkTests::GameModeResolution::GetBundleExperiencePath());
		ASSERT_THAT(IsNotNull(FallbackExperience));

		TestRunner->AddExpectedError(TEXT("No ArcMassExperienceDefinition asset named"), EAutomationExpectedErrorFlags::Contains, 1);
		GameMode.OptionsString = TEXT("?Experience=NoSuchExperienceAsset");
		FCommandLine::Append(*FString::Printf(TEXT(" -Experience=%s"), *FallbackExperience->GetName()));

		FString Source;
		const FPrimaryAssetId Id =
			ArcMassFrameworkTests::GameModeResolution::CallResolver(GameMode, Source);

		ASSERT_THAT(IsFalse(Id.IsValid()));
		ASSERT_THAT(AreEqual(FString(TEXT("OptionsString")), Source));
	}

	TEST_METHOD(Resolution_NoneSet_ReturnsInvalid)
	{
		AArcMassGameMode& GameMode = Spawner.SpawnActor<AArcMassGameMode>();

		GameMode.OptionsString.Reset();
		FCommandLine::Set(*OriginalCommandLine);

		FString IgnoreScratch;
		if (FParse::Value(FCommandLine::Get(), TEXT("Experience="), IgnoreScratch))
		{
			AddInfo(TEXT("Skipping Resolution_NoneSet_ReturnsInvalid: launcher command line carries -Experience=."));
			return;
		}

		// Leave WorldSettings DefaultExperience null (default-constructed).

		FString Source;
		const FPrimaryAssetId Id =
			ArcMassFrameworkTests::GameModeResolution::CallResolver(GameMode, Source);

		ASSERT_THAT(IsFalse(Id.IsValid()));
		ASSERT_THAT(IsTrue(Source.IsEmpty()));
	}
};
