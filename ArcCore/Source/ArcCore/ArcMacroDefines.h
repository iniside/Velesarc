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
#include "Containers/Map.h"
#include "Delegates/IDelegateInstance.h"
#include "UObject/Class.h"

#define DEFINE_ARC_DELEGATE(Type, DelegateName)                                  \
                                                                                 \
protected:                                                                       \
	mutable Type DelegateName##Delegate;                                         \
                                                                                 \
public:                                                                          \
	FDelegateHandle Add##DelegateName(typename Type::FDelegate InDelegate) const \
	{                                                                            \
		return DelegateName##Delegate.Add(InDelegate);                           \
	}                                                                            \
	void Remove##DelegateName(FDelegateHandle InHandle) const                    \
	{                                                                            \
		DelegateName##Delegate.Remove(InHandle);                                 \
	}                                                                            \
	void RemoveAll##DelegateName(const void* InUserObject) const                 \
	{                                                                            \
		DelegateName##Delegate.RemoveAll(InUserObject);                          \
	}                                                                            \
	template<typename... Args>													 \
	void Broadcast##DelegateName(Args... args) const							 \
	{																			 \
		DelegateName##Delegate. Broadcast(args...);								 \
	}																			 \
private:

#define DEFINE_ARC_DELEGATE_MAP(Key, DelegateType, DelegateName)           		\
                                                                           		\
private:                                                                   		\
	TMap<Key, DelegateType> DelegateName##DelegateMap;                     		\
                                                                           		\
public:                                                                    		\
	FDelegateHandle Add##DelegateName##Map(                                		\
		Key InKey, const typename DelegateType::FDelegate& InDelegate)     		\
	{                                                                      		\
		return DelegateName##DelegateMap.FindOrAdd(InKey).Add(InDelegate); 		\
	}                                                                      		\
	void Remove##DelegateName##Map(Key InKey, FDelegateHandle InHandle)    		\
	{                                                                      		\
		if (DelegateType* D = DelegateName##DelegateMap.Find(InKey))       		\
		{                                                                  		\
			D->Remove(InHandle);                                           		\
		}                                                                  		\
	}                                                                      		\
	template<typename... Args>													\
	void Broadcast##DelegateName##Map(Key InKey, Args... args) const			\
	{																			\
		if (const DelegateType* Del = DelegateName##DelegateMap.Find(InKey))	\
		{																		\
			Del->Broadcast(args...);											\
		}																		\
	}																			\
private:


template<typename ChannelType, typename DelegateType>
class TArcChannelDelegate
{
private:
	mutable TMap<ChannelType, DelegateType> Delegates;
	
public:
	FDelegateHandle AddDelegate(ChannelType InChannel, const typename DelegateType::FDelegate& InDelegate) const
	{
		return Delegates.FindOrAdd(InChannel).Add(InDelegate);
	}

	void RemoveDelegate(ChannelType InChannel, FDelegateHandle InHandle) const
	{
		if (DelegateType* Del = Delegates.Find(InChannel))
		{
			Del->Remove(InHandle);
		}
	}

	template<typename... Args>
	void Broadcast(ChannelType InChannel, Args... args) const
	{
		if (DelegateType* Del = Delegates.Find(InChannel))
		{
			Del->Broadcast(args...);
		}
	}
};
#define DEFINE_ARC_CHANNEL_DELEGATE(ChannelKeyType, FriendlyName, DelegateType, DelegateName)           \
private:																								\
TArcChannelDelegate<ChannelKeyType, DelegateType> FriendlyName##DelegateName##Delegates;				\
public:																									\
FDelegateHandle Add##FriendlyName##DelegateName(ChannelKeyType InChannelKey								\
, const typename DelegateType::FDelegate& InDelegate) const												\
{																										\
	return FriendlyName##DelegateName##Delegates.AddDelegate(InChannelKey, InDelegate);					\
}                                                                            							\
void Remove##FriendlyName##DelegateName(ChannelKeyType InChannelKey, FDelegateHandle InHandle) const    \
{																										\
	FriendlyName##DelegateName##Delegates.RemoveDelegate(InChannelKey, InHandle);						\
}																										\
template<typename... Args>																				\
void Broadcast##FriendlyName##DelegateName(ChannelKeyType InChannel, Args... args) const				\
{																										\
	FriendlyName##DelegateName##Delegates.Broadcast(InChannel, args...);								\
}																										\
private:

template<typename ChannelType, typename KeyType, typename DelegateType>
class TArcChannelDelegateMap
{
private:
	struct Channel
	{
		TMap<KeyType, DelegateType> Delegates;	
	};
	TMap<ChannelType, Channel> Channels;
	
public:
	FDelegateHandle AddDelegate(ChannelType InChannel
		, KeyType InKey
		, const typename DelegateType::FDelegate& InDelegate)
	{
		Channel& Ref = Channels.FindOrAdd(InChannel); 
		return Ref.Delegates.FindOrAdd(InKey).Add(InDelegate);
	}

	void RemoveDelegate(ChannelType InChannel, KeyType InKey, FDelegateHandle InHandle)
	{
		if (Channel* Del = Channels.Find(InChannel))
		{
			if (DelegateType* Delegate = Del->Delegates.Find(InKey))
			{
				Delegate->Remove(InHandle);
				if (Delegate->IsBound() == false)
				{
					Del->Delegates.Remove(InKey);
					if (Del->Delegates.Num() == 0)
					{
						Channels.Remove(InChannel);
					}
				}
			}
		}
	}

	template<typename... Args>
	void Broadcast(ChannelType InChannel
		, KeyType InKey
		, Args... args) const
	{
		if (const Channel* Chan = Channels.Find(InChannel))
		{
			if (const DelegateType* Delegate = Chan->Delegates.Find(InKey))
			{
				Delegate->Broadcast(args...);
			}
		}
	}
};

#define DEFINE_ARC_CHANNEL_DELEGATE_MAP(ChannelKeyType, FriendlyName, Key, DelegateType, DelegateName)			\
TArcChannelDelegateMap<ChannelKeyType, Key, DelegateType> DelegateName##FriendlyName##DelegateMap;              \
public:																											\
FDelegateHandle Add##FriendlyName##DelegateName##Map(															\
ChannelKeyType InChannel, Key InKey, const typename DelegateType::FDelegate& InDelegate)						\
{																												\
	return DelegateName##FriendlyName##DelegateMap.AddDelegate(InChannel, InKey, InDelegate);					\
}																												\
void Remove##FriendlyName##DelegateName##Map(ChannelKeyType InChannel, Key InKey, FDelegateHandle InHandle)		\
{																												\
	DelegateName##FriendlyName##DelegateMap.RemoveDelegate(InChannel, InKey, InHandle);							\
}																												\
template<typename... Args>																						\
void Broadcast##FriendlyName##DelegateName##Map(ChannelKeyType InChannel, Key InKey, Args... args) const		\
{																												\
	DelegateName##FriendlyName##DelegateMap.Broadcast(InChannel, InKey, args...);								\
}																												\
private:


template<typename ClassChannelType, typename DelegateType>
class TArcClassChannelDelegate
{
	
private:
	struct Channel
	{
		UClass* ChannelClass = nullptr;
		DelegateType Delegate;

		bool operator==(const Channel& Other) const
		{
			return ChannelClass == Other.ChannelClass;
		}

		bool operator==(const UClass* InChannelClass) const
		{
			return ChannelClass == InChannelClass;
		}

		friend uint32 GetTypeHash(const Channel& InKey)
		{
			return GetTypeHash(InKey.ChannelClass);
		}
	};
	
private:
	//mutable TArray<Channel> Delegates;
	
	mutable TMap<UClass*, DelegateType> Delegates;
	
public:
	FDelegateHandle AddDelegate(ClassChannelType InChannel, const typename DelegateType::FDelegate& InDelegate) const
	{
		//int32 ExistingIdx = Delegates.IndexOfByKey(InChannel);
		//if (ExistingIdx != INDEX_NONE)
		//{
		//	return Delegates[ExistingIdx].Delegate.Add(InDelegate);
		//}
		//
		//Channel NewChannel { InChannel };
		//int32 NewIdx = Delegates.Add(NewChannel);
		DelegateType* FoundDelegate = Delegates.Find(InChannel);
		if (FoundDelegate == nullptr)
		{
			UClass* ChannelClass = InChannel;
			UClass* SuperClass = ChannelClass->GetSuperClass();
			while (SuperClass != nullptr)
			{
				FoundDelegate = Delegates.Find(SuperClass);
				if (FoundDelegate != nullptr)
				{
					break;
				}
				SuperClass = SuperClass->GetSuperClass();
			}
		}

		if (FoundDelegate == nullptr)
		{
			DelegateType& NewDelegate = Delegates.Add(InChannel);
			return NewDelegate.Add(InDelegate);
		}

		return FDelegateHandle();
		//return Delegates[NewIdx].Delegate.Add(InDelegate);
	}

	void RemoveDelegate(ClassChannelType InChannel, FDelegateHandle InHandle) const
	{
		//int32 ExistingIdx = Delegates.IndexOfByKey(InChannel);
		//if (ExistingIdx != INDEX_NONE)
		//{
		//	Delegates[ExistingIdx].Delegate.Remove(InHandle);
		//}
		
		DelegateType* FoundDelegate = Delegates.Find(InChannel);
		if (FoundDelegate == nullptr)
		{
			UClass* ChannelClass = InChannel;
			UClass* SuperClass = ChannelClass->GetSuperClass();
			while (SuperClass != nullptr)
			{
				FoundDelegate = Delegates.Find(SuperClass);
				if (FoundDelegate != nullptr)
				{
					break;
				}
				SuperClass = SuperClass->GetSuperClass();
			}
		}

		if (FoundDelegate)
		{
			FoundDelegate->Remove(InHandle);
		}
	}

	template<typename... Args>
	void Broadcast(ClassChannelType InChannel, Args... args) const
	{
		//for (const Channel& Chan : Delegates)
		//{
		//	if (InChannel->IsChildOf(Chan.ChannelClass))
		//	{
		//		Chan.Delegate.Broadcast(args...);
		//		return;
		//	}
		//}

		DelegateType* FoundDelegate = Delegates.Find(InChannel);
		if (FoundDelegate == nullptr)
		{
			UClass* ChannelClass = InChannel;
			UClass* SuperClass = ChannelClass->GetSuperClass();
			while (SuperClass != nullptr)
			{
				FoundDelegate = Delegates.Find(SuperClass);
				if (FoundDelegate != nullptr)
				{
					break;
				}
				SuperClass = SuperClass->GetSuperClass();
			}
		}

		if (FoundDelegate != nullptr)
		{
			FoundDelegate->Broadcast(args...);
		};
	}
};
#define DEFINE_ARC_CHANNEL_CLASS_DELEGATE(ChannelClassType, FriendlyName, DelegateType, DelegateName)           \
private:																								\
TArcClassChannelDelegate<ChannelClassType, DelegateType> FriendlyName##DelegateName##Delegates;				\
public:																									\
FDelegateHandle Add##FriendlyName##DelegateName(ChannelClassType InChannelKey								\
, const typename DelegateType::FDelegate& InDelegate) const												\
{																										\
return FriendlyName##DelegateName##Delegates.AddDelegate(InChannelKey, InDelegate);					\
}                                                                            							\
void Remove##FriendlyName##DelegateName(ChannelClassType InChannelKey, FDelegateHandle InHandle) const    \
{																										\
FriendlyName##DelegateName##Delegates.RemoveDelegate(InChannelKey, InHandle);						\
}																										\
template<typename... Args>																				\
void Broadcast##FriendlyName##DelegateName(ChannelClassType InChannel, Args... args) const				\
{																										\
FriendlyName##DelegateName##Delegates.Broadcast(InChannel, args...);								\
}																										\
private: