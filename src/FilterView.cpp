/*
 * Copyright 2025-2026, Dru Satori @ Druware Software Designs.
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Logger
 * The bar where the user edits the filter rules.
 */


#include "FilterView.h"

#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TextControl.h>

#include "LogFilter.h"
#include "LogRecord.h"


enum {
	kMsgAddRule		= 'adrl',
	kMsgClearRules	= 'clrl',
	kMsgRemoveRule	= 'rmrl',
	kMsgRuleChanged	= 'rlch'
};


// One rule: [x] [column] [operator] [value] [Match case] [Remove]
class FilterRuleView : public BGroupView {
public:
								FilterRuleView(BHandler* target);

	virtual	void				AttachedToWindow();

			void				SetTo(int32 field, int32 op,
									const char* value, bool caseSensitive);

			bool				IsEnabled() const;
			int32				Field() const;
			int32				Operator() const;
			const char*			Value() const;
			bool				IsCaseSensitive() const;

			void				MarkAsInvalid(bool invalid);
			void				FocusValue();

private:
			void				_AddFieldItem(int32 field);

			BHandler*			fTarget;
			BCheckBox*			fEnabledBox;
			BPopUpMenu*			fFieldMenu;
			BPopUpMenu*			fOperatorMenu;
			BTextControl*		fValueControl;
			BCheckBox*			fCaseBox;
			BButton*			fRemoveButton;
};


FilterRuleView::FilterRuleView(BHandler* target)
	:
	BGroupView("rule", B_HORIZONTAL, B_USE_HALF_ITEM_SPACING),
	fTarget(target)
{
	fEnabledBox = new BCheckBox("enabled", NULL,
		new BMessage(kMsgRuleChanged));
	fEnabledBox->SetValue(B_CONTROL_ON);
	fEnabledBox->SetToolTip("Use this rule");

	fFieldMenu = new BPopUpMenu("column");
	_AddFieldItem(kFieldAny);
	_AddFieldItem(kFieldRaw);
	fFieldMenu->AddSeparatorItem();
	for (int32 field = 0; field < kFieldCount; field++)
		_AddFieldItem(field);

	fOperatorMenu = new BPopUpMenu("operator");
	for (int32 op = 0; op < kOpCount; op++) {
		fOperatorMenu->AddItem(new BMenuItem(OperatorName(op),
			new BMessage(kMsgRuleChanged)));
	}

	fValueControl = new BTextControl("value", NULL, "", NULL);
	fValueControl->SetModificationMessage(new BMessage(kMsgRuleChanged));

	fCaseBox = new BCheckBox("case", "Match case",
		new BMessage(kMsgRuleChanged));

	BMessage* removeMessage = new BMessage(kMsgRemoveRule);
	removeMessage->AddPointer("rule", this);
	fRemoveButton = new BButton("remove", "Remove", removeMessage);

	BLayoutBuilder::Group<>(this)
		.Add(fEnabledBox)
		.Add(new BMenuField("column", NULL, fFieldMenu))
		.Add(new BMenuField("operator", NULL, fOperatorMenu))
		.Add(fValueControl, 10.0f)
		.Add(fCaseBox)
		.Add(fRemoveButton);

	SetTo(kFieldMessage, kOpContains, "", false);
}


void
FilterRuleView::AttachedToWindow()
{
	BGroupView::AttachedToWindow();

	fEnabledBox->SetTarget(fTarget);
	fFieldMenu->SetTargetForItems(fTarget);
	fOperatorMenu->SetTargetForItems(fTarget);
	fValueControl->SetTarget(fTarget);
	fCaseBox->SetTarget(fTarget);
	fRemoveButton->SetTarget(fTarget);
}


void
FilterRuleView::SetTo(int32 field, int32 op, const char* value,
	bool caseSensitive)
{
	for (int32 i = 0; BMenuItem* item = fFieldMenu->ItemAt(i); i++) {
		int32 itemField;
		if (item->Message() != NULL
			&& item->Message()->FindInt32("field", &itemField) == B_OK
			&& itemField == field) {
			item->SetMarked(true);
		}
	}
	if (BMenuItem* item = fOperatorMenu->ItemAt(op))
		item->SetMarked(true);
	fValueControl->SetText(value);
	fCaseBox->SetValue(caseSensitive ? B_CONTROL_ON : B_CONTROL_OFF);
}


bool
FilterRuleView::IsEnabled() const
{
	return fEnabledBox->Value() == B_CONTROL_ON;
}


int32
FilterRuleView::Field() const
{
	int32 field = kFieldMessage;
	BMenuItem* item = fFieldMenu->FindMarked();
	if (item != NULL && item->Message() != NULL)
		item->Message()->FindInt32("field", &field);
	return field;
}


int32
FilterRuleView::Operator() const
{
	int32 index = fOperatorMenu->IndexOf(fOperatorMenu->FindMarked());
	return index >= 0 ? index : kOpContains;
}


const char*
FilterRuleView::Value() const
{
	return fValueControl->Text();
}


bool
FilterRuleView::IsCaseSensitive() const
{
	return fCaseBox->Value() == B_CONTROL_ON;
}


void
FilterRuleView::MarkAsInvalid(bool invalid)
{
	fValueControl->MarkAsInvalid(invalid);
	fValueControl->SetToolTip(invalid ? "Invalid regular expression" : NULL);
}


void
FilterRuleView::FocusValue()
{
	fValueControl->MakeFocus(true);
}


void
FilterRuleView::_AddFieldItem(int32 field)
{
	BMessage* message = new BMessage(kMsgRuleChanged);
	message->AddInt32("field", field);
	fFieldMenu->AddItem(new BMenuItem(FieldName(field), message));
}


// #pragma mark - FilterView


FilterView::FilterView(BMessenger target)
	:
	BGroupView("filters", B_VERTICAL, B_USE_HALF_ITEM_SPACING),
	fTarget(target)
{
	fMatchMenu = new BPopUpMenu("match");
	fMatchMenu->AddItem(new BMenuItem("all", new BMessage(kMsgRuleChanged)));
	fMatchMenu->AddItem(new BMenuItem("any", new BMessage(kMsgRuleChanged)));
	fMatchMenu->ItemAt(0)->SetMarked(true);
	fMatchField = new BMenuField("match", "Show lines matching", fMatchMenu);

	fAddButton = new BButton("add", "Add rule", new BMessage(kMsgAddRule));
	fClearButton = new BButton("clear", "Remove all",
		new BMessage(kMsgClearRules));
	fClearButton->SetEnabled(false);

	fRulesView = new BGroupView("rules", B_VERTICAL, B_USE_HALF_ITEM_SPACING);

	BLayoutBuilder::Group<>(this)
		.SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING,
			B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(fMatchField)
			.Add(new BStringView("of", "of these rules:"))
			.AddGlue()
			.Add(fAddButton)
			.Add(fClearButton)
		.End()
		.Add(fRulesView);
}


void
FilterView::AttachedToWindow()
{
	BGroupView::AttachedToWindow();

	fMatchMenu->SetTargetForItems(this);
	fAddButton->SetTarget(this);
	fClearButton->SetTarget(this);
}


void
FilterView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgAddRule:
			_AddRuleView()->FocusValue();
			_Changed();
			break;

		case kMsgClearRules:
			RemoveAllRules();
			break;

		case kMsgRemoveRule:
		{
			void* rule;
			if (message->FindPointer("rule", &rule) != B_OK)
				break;
			for (int32 i = 0; BView* child = fRulesView->ChildAt(i); i++) {
				if (child == rule) {
					child->RemoveSelf();
					delete child;
					_Changed();
					break;
				}
			}
			break;
		}

		case kMsgRuleChanged:
			_Changed();
			break;

		default:
			BGroupView::MessageReceived(message);
	}
}


void
FilterView::AddRule(int32 field, int32 op, const char* value,
	bool caseSensitive)
{
	_AddRuleView()->SetTo(field, op, value, caseSensitive);
	_Changed();
}


void
FilterView::RemoveAllRules()
{
	while (BView* child = fRulesView->ChildAt(0)) {
		child->RemoveSelf();
		delete child;
	}
	_Changed();
}


int32
FilterView::CountRules() const
{
	return fRulesView->CountChildren();
}


void
FilterView::GetFilters(FilterSet& filters)
{
	filters.MakeEmpty();
	filters.SetMatchAll(fMatchMenu->FindMarkedIndex() != 1);

	for (int32 i = 0; BView* child = fRulesView->ChildAt(i); i++) {
		FilterRuleView* rule = dynamic_cast<FilterRuleView*>(child);
		if (rule == NULL)
			continue;
		if (!rule->IsEnabled()) {
			rule->MarkAsInvalid(false);
			continue;
		}
		status_t status = filters.AddRule(rule->Field(), rule->Operator(),
			rule->Value(), rule->IsCaseSensitive());
		rule->MarkAsInvalid(status != B_OK);
	}
}


FilterRuleView*
FilterView::_AddRuleView()
{
	FilterRuleView* rule = new FilterRuleView(this);
	fRulesView->GroupLayout()->AddView(rule);
	return rule;
}


void
FilterView::_Changed()
{
	fClearButton->SetEnabled(CountRules() > 0);
	fTarget.SendMessage(kMsgFiltersChanged);
}
