// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
class FArcRmlFileInterface;
struct FArcRmlUIThreadDependencies;

namespace Rml
{
	class ElementDocument;
}

namespace ArcRmlLayoutDump
{
	struct FResult
	{
		FString LayoutPath;
		FString HtmlPath;
		int32 Elements = 0;
	};

	bool Write(Rml::ElementDocument& Document, const FString& DocumentPath, TConstArrayView<FString> StyleSheetLinks, const FArcRmlFileInterface& FileInterface,
		const FArcRmlUIThreadDependencies& Dependencies, const FString& OutDir, FResult& OutResult);
}
#endif
