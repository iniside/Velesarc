#include "DataView.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "ArcPerfInternal.h"
#include <algorithm>

namespace Rml {

DataView::~DataView() {}

Element* DataView::GetElement() const
{
	Element* result = attached_element.get();
	if (!result)
		Log::Message(Log::LT_WARNING, "Could not retrieve element in view, was it destroyed?");
	return result;
}

int DataView::GetSortOrder() const
{
	return sort_order;
}

bool DataView::IsValid() const
{
	return static_cast<bool>(attached_element);
}

DataView::DataView(Element* element, int bias) : attached_element(element->GetObserverPtr()), sort_order(bias + 1000)
{
	RMLUI_ASSERT(bias >= -1000 && bias <= 999);

	if (element)
	{
		for (Element* parent = element->GetParentNode(); parent; parent = parent->GetParentNode())
			sort_order += 2000;
	}
}

DataViews::DataViews() {}

DataViews::~DataViews() {}

void DataViews::Add(DataViewPtr view)
{
	DataView* raw = view.get();
	DataView*& head = element_views.emplace(raw->attached_element.get(), nullptr).first->second;
	raw->next_on_element = head;
	head = raw;
	raw->pending = true;
	raw->list_index = views_to_add.size();
	views_to_add.push_back(std::move(view));
}

DataViewPtr DataViews::TakeView(DataViewList& list, size_t index)
{
	DataViewPtr view = std::move(list[index]);
	if (index + 1 != list.size())
	{
		list[index] = std::move(list.back());
		list[index]->list_index = index;
	}
	list.pop_back();
	return view;
}

void DataViews::OnElementRemove(Element* element)
{
	const UnorderedMap<Element*, DataView*>::iterator it = element_views.find(element);
	if (it == element_views.end())
		return;

	DataView* view = it->second;
	element_views.erase(it);
	while (view)
	{
		DataView* next = view->next_on_element;
		view->next_on_element = nullptr;
		ForgetView(view);
		if (view->pending)
		{
			TakeView(views_to_add, view->list_index);
		}
		else
		{
			view->removed = true;
			views_to_remove.push_back(TakeView(views, view->list_index));
		}
		view = next;
	}
}

void DataViews::OnElementRebind(Element* element)
{
	const UnorderedMap<Element*, DataView*>::iterator it = element_views.find(element);
	if (it != element_views.end())
	{
		for (DataView* view = it->second; view; view = view->next_on_element)
			view->RestoreElementSource();
	}
	OnElementRemove(element);
}

void DataViews::ForgetView(DataView* view)
{
	if (view->update_requested)
	{
		view->update_requested = false;
		requested_views.erase(std::find(requested_views.begin(), requested_views.end(), view));
	}
	if (view->layout_observer)
	{
		view->layout_observer = false;
		layout_views.erase(std::find(layout_views.begin(), layout_views.end(), view));
	}
}

void DataViews::RequestUpdate(DataView* view)
{
	if (view->update_requested || view->removed)
		return;
	view->update_requested = true;
	requested_views.push_back(view);
}

void DataViews::AddLayoutObserver(DataView* view)
{
	if (view->layout_observer || view->removed)
		return;
	view->layout_observer = true;
	layout_views.push_back(view);
}

bool DataViews::UpdateAfterLayout(DataModel& model)
{
	bool result = false;
	for (size_t i = 0; i < layout_views.size(); i++)
	{
		DataView* view = layout_views[i];
		if (!view->removed && view->IsValid())
			result |= view->OnLayout(model);
	}
	return result;
}

void DataViews::ReleaseRemovedViews()
{
	if (views_to_remove.empty())
		return;

	UnorderedSet<DataView*> removed;
	UnorderedSet<String> names;
	removed.reserve(views_to_remove.size());
	for (const DataViewPtr& view : views_to_remove)
	{
		removed.insert(view.get());
		view->ForEachVariableAddress([&names](const DataAddress& address) { names.insert(address.front().name); });
	}

	for (const String& variable_name : names)
	{
		const std::pair<NameViewMap::iterator, NameViewMap::iterator> range = name_view_map.equal_range(variable_name);
		for (NameViewMap::iterator it = range.first; it != range.second;)
		{
			if (removed.count(it->second) == 1)
				it = name_view_map.erase(it);
			else
				++it;
		}
	}

	views_to_remove.clear();
}

bool DataViews::Update(DataModel& model, const DirtyVariables& dirty_variables)
{
	constexpr int max_passes = 1000;

	bool result = false;
	size_t num_dirty_variables_prev = 0;

	// Passes repeat until no views are added, so nested structural views settle within one call; dirty variable views rerun only for newly dirtied variables.
	for (int pass = 0; pass == 0 || !views_to_add.empty() || num_dirty_variables_prev != dirty_variables.size(); pass++)
	{
		if (pass == max_passes)
		{
			Log::Message(Log::LT_ERROR, "Data views did not settle after %d update passes; remaining views are updated next time.", max_passes);
			break;
		}

		const bool run_dirty_variables = (pass == 0 || num_dirty_variables_prev != dirty_variables.size());
		num_dirty_variables_prev = dirty_variables.size();

		ReleaseRemovedViews();

		Vector<DataView*> dirty_views;

		if (!views_to_add.empty())
		{
			views.reserve(views.size() + views_to_add.size());
			for (DataViewPtr& view : views_to_add)
			{
				DataView* added_view = view.get();
				dirty_views.push_back(added_view);
				added_view->ForEachVariableAddress(
					[this, added_view](const DataAddress& address) { name_view_map.emplace(address.front().name, added_view); });

				view->pending = false;
				view->list_index = views.size();
				views.push_back(std::move(view));
			}
			views_to_add.clear();
		}

		if (pass == 0)
		{
			for (DataView* requested_view : requested_views)
			{
				requested_view->update_requested = false;
				dirty_views.push_back(requested_view);
			}
			requested_views.clear();
		}

		if (run_dirty_variables)
		{
			for (const String& variable_name : dirty_variables)
			{
				const std::pair<NameViewMap::iterator, NameViewMap::iterator> pair = name_view_map.equal_range(variable_name);
				for (NameViewMap::iterator it = pair.first; it != pair.second; ++it)
					dirty_views.push_back(it->second);
			}
		}

		// Remove duplicate entries
		std::sort(dirty_views.begin(), dirty_views.end());
		Vector<DataView*>::iterator it_remove = std::unique(dirty_views.begin(), dirty_views.end());
		dirty_views.erase(it_remove, dirty_views.end());

		// Sort by the element's depth in the document tree so that any structural changes due to a changed variable are reflected in the element's
		// children. Eg. the 'data-for' view will remove children if any of its data variable array size is reduced.
		std::sort(dirty_views.begin(), dirty_views.end(), [](const DataView* left, const DataView* right) { return left->GetSortOrder() < right->GetSortOrder(); });

		for (DataView* view : dirty_views)
		{
			RMLUI_ASSERT(view);
			if (!view || view->removed)
				continue;

			if (view->IsValid())
			{
				RMLUI_ARC_PERF_SCOPE(DataViewUpdate);
				RMLUI_ARC_PERF_COUNT(DataViewsUpdated, 1);
				RMLUI_ARC_PERF_CODE(const ArcPerf::Stopwatch arc_perf_stopwatch(ArcPerf::Detailed());)
				result |= view->Update(model);
				RMLUI_ARC_PERF_CODE({
					if (ArcPerf::Detailed())
					{
						String variables;
						view->ForEachVariableAddress([&variables](const DataAddress& address) {
							variables += (variables.empty() ? "" : ",") + address.front().name;
						});
						ArcPerf::RecordNamed(ArcPerf::NamedList::DataViewVariables, variables.empty() ? String("(no variables)") : variables,
							arc_perf_stopwatch.ElapsedMs(), 1);
					}
				})
			}
		}
	}

	ReleaseRemovedViews();

	return result;
}

} // namespace Rml
