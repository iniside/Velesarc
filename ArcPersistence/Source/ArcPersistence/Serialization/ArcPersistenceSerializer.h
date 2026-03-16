/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "CoreMinimal.h"

class FArcSaveArchive;
class FArcLoadArchive;
class UWorld;

// ─────────────────────────────────────────────────────────────────────────────
// Function pointer types for external serializers
// ─────────────────────────────────────────────────────────────────────────────

using FArcPersistenceSaveFunc = TFunction<void(const void* Source, FArcSaveArchive& Ar)>;
using FArcPersistenceLoadFunc = TFunction<void(void* Target, FArcLoadArchive& Ar)>;
using FArcPersistencePreSaveFunc  = void(*)(const void* Source, UWorld& World);
using FArcPersistencePostLoadFunc = void(*)(void* Target, UWorld& World);

// ─────────────────────────────────────────────────────────────────────────────
// FArcPersistenceSerializerInfo — runtime descriptor for a registered serializer
// ─────────────────────────────────────────────────────────────────────────────

struct ARCPERSISTENCE_API FArcPersistenceSerializerInfo
{
	FName StructName;
	uint32 Version = 0;
	bool bSupportsTombstones = false;
	FArcPersistenceSaveFunc SaveFunc = nullptr;
	FArcPersistenceLoadFunc LoadFunc = nullptr;
	FArcPersistencePreSaveFunc  PreSaveFunc  = nullptr;
	FArcPersistencePostLoadFunc PostLoadFunc = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// SFINAE helper — detects bSupportsTombstones static member
// ─────────────────────────────────────────────────────────────────────────────

template<typename U, typename = void>
struct TArcHasTombstoneSupport
{
	static constexpr bool Value = false;
};

template<typename U>
struct TArcHasTombstoneSupport<U, std::void_t<decltype(U::bSupportsTombstones)>>
{
	static constexpr bool Value = U::bSupportsTombstones;
};

// ─────────────────────────────────────────────────────────────────────────────
// SFINAE helper — detects static PreSave(const SourceType&, UWorld&)
// ─────────────────────────────────────────────────────────────────────────────

template<typename U, typename = void>
struct TArcHasPreSave
{
	static constexpr bool Value = false;
};

template<typename U>
struct TArcHasPreSave<U, std::void_t<decltype(U::PreSave(
	std::declval<const typename U::SourceType&>(),
	std::declval<UWorld&>()))>>
{
	static constexpr bool Value = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// SFINAE helper — detects static PostLoad(SourceType&, UWorld&)
// ─────────────────────────────────────────────────────────────────────────────

template<typename U, typename = void>
struct TArcHasPostLoad
{
	static constexpr bool Value = false;
};

template<typename U>
struct TArcHasPostLoad<U, std::void_t<decltype(U::PostLoad(
	std::declval<typename U::SourceType&>(),
	std::declval<UWorld&>()))>>
{
	static constexpr bool Value = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// TArcPersistenceSerializerTraits — builds info from a user's serializer struct
// ─────────────────────────────────────────────────────────────────────────────

template<typename T>
struct TArcPersistenceSerializerTraits
{
	static FArcPersistenceSerializerInfo Build(FName InStructName)
	{
		FArcPersistenceSerializerInfo Info;
		Info.StructName = InStructName;
		Info.Version = T::Version;
		Info.bSupportsTombstones = TArcHasTombstoneSupport<T>::Value;
		Info.SaveFunc = [](const void* Source, FArcSaveArchive& Ar)
		{
			T::Save(*static_cast<const typename T::SourceType*>(Source), Ar);
		};
		Info.LoadFunc = [](void* Target, FArcLoadArchive& Ar)
		{
			T::Load(*static_cast<typename T::SourceType*>(Target), Ar);
		};

		if constexpr (TArcHasPreSave<T>::Value)
		{
			Info.PreSaveFunc = [](const void* Source, UWorld& World)
			{
				T::PreSave(*static_cast<const typename T::SourceType*>(Source), World);
			};
		}

		if constexpr (TArcHasPostLoad<T>::Value)
		{
			Info.PostLoadFunc = [](void* Target, UWorld& World)
			{
				T::PostLoad(*static_cast<typename T::SourceType*>(Target), World);
			};
		}

		return Info;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// FArcPersistenceSerializerRegistryDelegates — auto-registration base class
// ─────────────────────────────────────────────────────────────────────────────

class ARCPERSISTENCE_API FArcPersistenceSerializerRegistryDelegates
{
public:
	FArcPersistenceSerializerRegistryDelegates();
	virtual ~FArcPersistenceSerializerRegistryDelegates();

	virtual void OnRegisterSerializers() {}
	virtual void OnUnregisterSerializers() {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Free functions used by registration macros
// ─────────────────────────────────────────────────────────────────────────────

namespace ArcPersistence
{
	ARCPERSISTENCE_API void RegisterSerializer(FName StructName, const FArcPersistenceSerializerInfo& Info);
	ARCPERSISTENCE_API void UnregisterSerializer(FName StructName);
}

// ─────────────────────────────────────────────────────────────────────────────
// Registration macros
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Header macro — declares the registration delegate struct for an external serializer.
 * Place in the header file after defining your serializer struct.
 *
 * @param SerializerType  The serializer struct type (e.g., FArcMySerializer)
 * @param Api             The module API macro (e.g., ARCPERSISTENCE_API)
 */
#define UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(SerializerType, Api) \
	struct Api SerializerType##_RegistryDelegates final : public FArcPersistenceSerializerRegistryDelegates \
	{ \
		SerializerType##_RegistryDelegates(); \
		~SerializerType##_RegistryDelegates(); \
		static const FArcPersistenceSerializerInfo Info; \
		virtual void OnRegisterSerializers() override; \
		virtual void OnUnregisterSerializers() override; \
	};

/**
 * CPP macro — implements the delegate and creates the static instance that triggers registration.
 * Place in the cpp file for your serializer.
 *
 * @param SerializerType  The serializer struct type (e.g., FArcMySerializer)
 * @param StructName      The string name of the target struct (e.g., "FMyGameStruct")
 */
#define UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(SerializerType, StructName) \
	const FArcPersistenceSerializerInfo SerializerType##_RegistryDelegates::Info = \
		TArcPersistenceSerializerTraits<SerializerType>::Build(FName(TEXT(StructName))); \
	SerializerType##_RegistryDelegates::SerializerType##_RegistryDelegates() \
	{ \
		OnRegisterSerializers(); \
	} \
	SerializerType##_RegistryDelegates::~SerializerType##_RegistryDelegates() \
	{ \
		OnUnregisterSerializers(); \
	} \
	static SerializerType##_RegistryDelegates G##SerializerType##_RegistryDelegatesInstance; \
	void SerializerType##_RegistryDelegates::OnRegisterSerializers() \
	{ \
		ArcPersistence::RegisterSerializer(FName(TEXT(StructName)), Info); \
	} \
	void SerializerType##_RegistryDelegates::OnUnregisterSerializers() \
	{ \
		ArcPersistence::UnregisterSerializer(FName(TEXT(StructName))); \
	}
