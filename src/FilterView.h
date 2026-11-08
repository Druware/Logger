/*
 * Copyright 2025-2026, Dru Satori @ Druware Software Designs.
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Logger
 * The bar where the user edits the filter rules.
 */
#ifndef FILTER_VIEW_H
#define FILTER_VIEW_H


#include <GroupView.h>
#include <Messenger.h>


class BButton;
class BMenuField;
class BPopUpMenu;
class FilterSet;
class FilterRuleView;


// Sent to the target whenever the rules change.
const uint32 kMsgFiltersChanged = 'fich';


class FilterView : public BGroupView {
public:
								FilterView(BMessenger target);

	virtual	void				AttachedToWindow();
	virtual	void				MessageReceived(BMessage* message);

			void				AddRule(int32 field, int32 op,
									const char* value,
									bool caseSensitive = false);
			void				RemoveAllRules();
			int32				CountRules() const;

	// Fills filters with the enabled rules; marks those that are invalid.
			void				GetFilters(FilterSet& filters);

private:
			FilterRuleView*		_AddRuleView();
			void				_Changed();

			BMessenger			fTarget;
			BPopUpMenu*			fMatchMenu;
			BMenuField*			fMatchField;
			BButton*			fAddButton;
			BButton*			fClearButton;
			BGroupView*			fRulesView;
};


#endif	// FILTER_VIEW_H
