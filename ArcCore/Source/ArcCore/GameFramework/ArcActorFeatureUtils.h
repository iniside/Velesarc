// Copyright ArcGame. All Rights Reserved.

#pragma once

/**
 * ArcActorFeatureUtils — typed wrappers around UGameFrameworkComponentManager's init-state subscription API.
 *
 * Non-feature UObjects (components, subsystems, plain C++ objects) can use these helpers to subscribe to
 * a specific actor feature (identified by NAME_ActorFeatureName on the feature class) reaching a specific
 * GameplayTag init state, without casting to UObject* or building the delegate boilerplate manually.
 *
 * Usage:
 *   FDelegateHandle Handle = ArcActorFeatureUtils::RegisterFeatureInitStateDelegate<UArcMyFeatureComponent>(
 *       MyActor,
 *       ArcActorFeatureUtils::TFeatureDelegate<UArcMyFeatureComponent>::CreateUObject(this, &UMyClass::OnFeatureReady),
 *       TAG_InitState_GameplayReady,
 *       true,   // bCallImmediately
 *       true);  // bCallOnce — auto-unregisters after first matching callback
 *
 *   ArcActorFeatureUtils::UnregisterFeatureInitStateDelegate(MyActor, Handle);
 */

#include "Components/GameFrameworkComponentDelegates.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Delegates/Delegate.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "Templates/SharedPointer.h"

namespace ArcActorFeatureUtils
{
	template <typename TFeature>
	using TFeatureWithParamsDelegate = TDelegate<void(const FActorInitStateChangedParams&, TFeature*)>;

	template <typename TFeature>
	using TFeatureDelegate = TDelegate<void(TFeature*)>;

	/**
	 * Subscribe to a specific actor feature reaching a specific init state.
	 * The delegate fires with the typed implementer cast to TFeature*.
	 * If bCallOnce is true, the registration auto-unregisters after the first matching callback.
	 * This overload receives only the typed implementer (no params struct).
	 */
	template <typename TFeature>
	FDelegateHandle RegisterFeatureInitStateDelegate(
		AActor* Actor,
		TFeatureDelegate<TFeature>&& Delegate,
		const FGameplayTag& RequiredState,
		bool bCallImmediately = true,
		bool bCallOnce = false)
	{
		static_assert(TIsDerivedFrom<TFeature, UObject>::Value, "TFeature must be derived from UObject");

		UGameFrameworkComponentManager* GFCM = UGameFrameworkComponentManager::GetForActor(Actor);
		if (!ensure(GFCM))
		{
			return FDelegateHandle();
		}

		// Capture the bound UObject before moving the delegate.
		UObject* BoundObject = Delegate.GetUObject();

		TSharedRef<FDelegateHandle> HandleRef = MakeShared<FDelegateHandle>();

		TFeatureDelegate<TFeature> CapturedDelegate = MoveTemp(Delegate);
		TFunction<void(const FActorInitStateChangedParams&)> Wrapper =
			[bCallOnce, CapturedDelegate = MoveTemp(CapturedDelegate), HandleRef](const FActorInitStateChangedParams& Params)
		{
			TFeature* Feature = Cast<TFeature>(Params.Implementer);
			if (Feature != nullptr)
			{
				CapturedDelegate.ExecuteIfBound(Feature);
				if (bCallOnce)
				{
					UGameFrameworkComponentManager* GFCM_Local = UGameFrameworkComponentManager::GetForActor(Params.OwningActor.Get());
					if (GFCM_Local != nullptr)
					{
						GFCM_Local->UnregisterActorInitStateDelegate(Params.OwningActor.Get(), *HandleRef);
					}
				}
			}
		};

		FActorInitStateChangedDelegate WrapperDelegate;
		if (BoundObject != nullptr)
		{
			WrapperDelegate = FActorInitStateChangedDelegate::CreateWeakLambda(BoundObject, MoveTemp(Wrapper));
		}
		else
		{
			WrapperDelegate = FActorInitStateChangedDelegate::CreateLambda(MoveTemp(Wrapper));
		}

		FDelegateHandle ReturnedHandle = GFCM->RegisterAndCallForActorInitState(
			Actor,
			TFeature::NAME_ActorFeatureName,
			RequiredState,
			MoveTemp(WrapperDelegate),
			bCallImmediately);

		*HandleRef = ReturnedHandle;
		return ReturnedHandle;
	}

	/**
	 * Subscribe to a specific actor feature reaching a specific init state.
	 * The delegate fires with the full params struct and the typed implementer cast to TFeature*.
	 * If bCallOnce is true, the registration auto-unregisters after the first matching callback.
	 * This overload receives both the FActorInitStateChangedParams and the typed implementer.
	 */
	template <typename TFeature>
	FDelegateHandle RegisterFeatureInitStateDelegate(
		AActor* Actor,
		TFeatureWithParamsDelegate<TFeature>&& Delegate,
		const FGameplayTag& RequiredState,
		bool bCallImmediately = true,
		bool bCallOnce = false)
	{
		static_assert(TIsDerivedFrom<TFeature, UObject>::Value, "TFeature must be derived from UObject");

		UGameFrameworkComponentManager* GFCM = UGameFrameworkComponentManager::GetForActor(Actor);
		if (!ensure(GFCM))
		{
			return FDelegateHandle();
		}

		// Capture the bound UObject before moving the delegate.
		UObject* BoundObject = Delegate.GetUObject();

		TSharedRef<FDelegateHandle> HandleRef = MakeShared<FDelegateHandle>();

		TFeatureWithParamsDelegate<TFeature> CapturedDelegate = MoveTemp(Delegate);
		TFunction<void(const FActorInitStateChangedParams&)> Wrapper =
			[bCallOnce, CapturedDelegate = MoveTemp(CapturedDelegate), HandleRef](const FActorInitStateChangedParams& Params)
		{
			TFeature* Feature = Cast<TFeature>(Params.Implementer);
			if (Feature != nullptr)
			{
				CapturedDelegate.ExecuteIfBound(Params, Feature);
				if (bCallOnce)
				{
					UGameFrameworkComponentManager* GFCM_Local = UGameFrameworkComponentManager::GetForActor(Params.OwningActor.Get());
					if (GFCM_Local != nullptr)
					{
						GFCM_Local->UnregisterActorInitStateDelegate(Params.OwningActor.Get(), *HandleRef);
					}
				}
			}
		};

		FActorInitStateChangedDelegate WrapperDelegate;
		if (BoundObject != nullptr)
		{
			WrapperDelegate = FActorInitStateChangedDelegate::CreateWeakLambda(BoundObject, MoveTemp(Wrapper));
		}
		else
		{
			WrapperDelegate = FActorInitStateChangedDelegate::CreateLambda(MoveTemp(Wrapper));
		}

		FDelegateHandle ReturnedHandle = GFCM->RegisterAndCallForActorInitState(
			Actor,
			TFeature::NAME_ActorFeatureName,
			RequiredState,
			MoveTemp(WrapperDelegate),
			bCallImmediately);

		*HandleRef = ReturnedHandle;
		return ReturnedHandle;
	}

	/**
	 * Unregister a previously registered actor init-state delegate.
	 * Safe to call during world teardown (early-outs silently if GFCM is gone).
	 */
	inline void UnregisterFeatureInitStateDelegate(AActor* Actor, FDelegateHandle& Handle)
	{
		if (!ensure(Actor) || Actor->GetWorld() == nullptr)
		{
			return;
		}

		UGameFrameworkComponentManager* GFCM = UGameFrameworkComponentManager::GetForActor(Actor);
		if (GFCM == nullptr && Actor->GetWorld()->bIsTearingDown)
		{
			// World is tearing down — GFCM may already be gone; silent early-out.
			return;
		}

		if (!ensure(GFCM))
		{
			return;
		}

		GFCM->UnregisterActorInitStateDelegate(Actor, Handle);
	}

	/**
	 * Synchronously get the object that currently implements a given feature on Actor, if it has reached RequiredState.
	 * Returns nullptr if the feature is not yet initialized or Actor is invalid.
	 */
	template <typename TFeature>
	TFeature* GetImplementerForFeature(AActor* Actor, const FGameplayTag& RequiredState = FGameplayTag::EmptyTag)
	{
		static_assert(TIsDerivedFrom<TFeature, UObject>::Value, "TFeature must be derived from UObject");

		if (!IsValid(Actor))
		{
			return nullptr;
		}

		UWorld* World = Actor->GetWorld();
		if (World == nullptr)
		{
			return nullptr;
		}

		UGameFrameworkComponentManager* GFCM = UGameFrameworkComponentManager::GetForActor(Actor);
		if (GFCM == nullptr)
		{
			return nullptr;
		}

		return Cast<TFeature>(GFCM->GetImplementerForFeature(Actor, TFeature::NAME_ActorFeatureName, RequiredState));
	}

} // namespace ArcActorFeatureUtils
