#pragma once

#include "../../Include/RmlUi/Core/DataTypes.h"
#include "../../Include/RmlUi/Core/Header.h"
#include "../../Include/RmlUi/Core/Traits.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Element;
class DataModel;

class DataViewInstancer : public NonCopyMoveable {
public:
	DataViewInstancer() {}
	virtual ~DataViewInstancer() {}
	virtual DataViewPtr InstanceView(Element* element) = 0;
};

template <typename T>
class DataViewInstancerDefault final : public DataViewInstancer {
public:
	DataViewPtr InstanceView(Element* element) override { return DataViewPtr(new T(element)); }
};

/**
    Data view.

    Data views are used to present a data variable in the document by different means.
    A data view is declared in the document by the element attribute:

        data-[type]-[modifier]="[expression]"

    The modifier may or may not be required depending on the data view.
 */

class DataView : public Releasable {
public:
	virtual ~DataView();

	// Initialize the data view.
	// @param[in] model The data model the view will be attached to.
	// @param[in] element The element which spawned the view.
	// @param[in] expression The value of the element's 'data-' attribute which spawned the view (see class documentation).
	// @param[in] modifier The modifier for the given view type (see class documentation).
	// @return True on success.
	virtual bool Initialize(DataModel& model, Element* element, const String& expression, const String& modifier) = 0;

	// Update the data view.
	// Returns true if the update resulted in a document change.
	virtual bool Update(DataModel& model) = 0;

	// Calls the function with each data address whose variable can modify this view.
	virtual void ForEachVariableAddress(const Function<void(const DataAddress&)>& function) const = 0;

	// Returns the attached element if it still exists.
	Element* GetElement() const;

	// Data views are first sorted by the depth of the attached element in the
	// document tree, then optionally by an offset specified for each data view.
	int GetSortOrder() const;

	// Returns true if the element still exists.
	bool IsValid() const;

	bool IsRemoved() const { return removed; }

	virtual void RestoreElementSource() {}

	virtual bool OnLayout(DataModel& /*model*/) { return false; }

protected:
	// @param[in] element The element this data view is attached to.
	// @param[in] sort_offset A number [-1000, 999] specifying the update order of this
	//            data view at the same tree depth, negative numbers are updated first.
	DataView(Element* element, int sort_offset);

private:
	friend class DataViews;

	ObserverPtr<Element> attached_element;
	int sort_order;

	DataView* next_on_element = nullptr;
	size_t list_index = 0;
	bool pending = false;
	bool removed = false;
	bool update_requested = false;
	bool layout_observer = false;
};

class DataViews : NonCopyMoveable {
public:
	DataViews();
	~DataViews();

	void Add(DataViewPtr view);

	void OnElementRemove(Element* element);
	void OnElementRebind(Element* element);

	bool Update(DataModel& model, const DirtyVariables& dirty_variables);

	bool HasPendingViews() const { return !views_to_add.empty(); }

	void RequestUpdate(DataView* view);
	void AddLayoutObserver(DataView* view);
	bool UpdateAfterLayout(DataModel& model);

private:
	using DataViewList = Vector<DataViewPtr>;

	static DataViewPtr TakeView(DataViewList& list, size_t index);
	void ReleaseRemovedViews();
	void ForgetView(DataView* view);

	DataViewList views;

	DataViewList views_to_add;
	DataViewList views_to_remove;

	Vector<DataView*> requested_views;
	Vector<DataView*> layout_views;

	UnorderedMap<Element*, DataView*> element_views;

	using NameViewMap = UnorderedMultimap<String, DataView*>;
	NameViewMap name_view_map;
};

} // namespace Rml
