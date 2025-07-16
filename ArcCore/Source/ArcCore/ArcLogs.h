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
#include "CoreMinimal.h"

class ArcLogs
{
public:
};

ARCCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogArcCore, Log, Log);

/*
 * Works only in Actor and ActorComponent.
 */
#if UE_BUILD_SHIPPING
	#define LOG_ARC_NET(LogCategory, Format, ...)
#else
#define LOG_ARC_NET(LogCategory, Format, ...)                                        \
		{                                                                                \
			ENetMode NM = GetNetMode();                                                  \
			switch (NM)                                                                  \
			{                                                                            \
				case NM_Standalone:                                                      \
					{                                                                    \
						FString LogText =                                                \
							FString::Printf(TEXT("Standlone: " #Format), ##__VA_ARGS__); \
						UE_LOG(LogCategory, Log, TEXT("%s"), *LogText);                  \
						break;															 \
					}                                                                    \
				case NM_DedicatedServer:                                                 \
					{                                                                    \
						FString LogText =                                                \
							FString::Printf(TEXT("Server: " #Format), ##__VA_ARGS__);    \
						UE_LOG(LogCategory, Log, TEXT("%s"), *LogText);                  \
						break;															 \
					}                                                                    \
				case NM_ListenServer:                                                    \
					{                                                                    \
						FString LogText = FString::Printf(                               \
							TEXT("ListenServer: " #Format), ##__VA_ARGS__);              \
						UE_LOG(LogCategory, Log, TEXT("%s"), *LogText);                  \
						break;															 \
					}                                                                    \
				case NM_Client:                                                          \
					{                                                                    \
						FString LogText =                                                \
							FString::Printf(TEXT("Client: " #Format), ##__VA_ARGS__);    \
						UE_LOG(LogCategory, Log, TEXT("%s"), *LogText);                  \
						break;															 \
					}                                                                    \
			}                                                                            \
		}
#endif
