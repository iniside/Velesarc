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


#include "Framework/Commands/Commands.h"
#include "Input/Reply.h"
#include "Misc/NotifyHook.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"

class FArcItemEditorCommands : public TCommands<FArcItemEditorCommands>
{
public:
	FArcItemEditorCommands();

	TSharedPtr<FUICommandInfo> CreateFrom;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

/** Main CollisionAnalyzer UI widget */
class SArcItemsConfigEditor
		: public SCompoundWidget
		, public FNotifyHook
{
	SLATE_USER_ARGS(SArcItemsConfigEditor)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
