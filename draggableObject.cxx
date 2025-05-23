#include "draggableObject.h"
#include "draggableObjectManager.h"

/*
* Initializer for DraggableObject. Parent is the top level NodePath of the object(s)
* wanting to be apart of the draggable object. traverse_num is how far down
* we traverse the children.
* 
* For example, if traverse_num==0: we only will pay attention to parent. However, if parent is just
* a holding node, i.e. empty, this is useless.
* 
* If traverse_num == 2, we will watch all our children (1) and all their children (2).
*/
DraggableObject::DraggableObject(NodePath& parent, int traverse_num) {
    watch_node_path(parent, traverse_num);
}

/*
* Initializer for DraggableObject. Watches for all nodes with the given tag name set via setTag.
* parent is the highest level node for which we seek tag. Pass render if nothing else.
*/

DraggableObject::DraggableObject(NodePath& parent, std::string tag) {
    _parent = parent;
    _tag = tag;
}

/*
* Blank initializer for DraggableObject. This is usually for when we want to set
* the nodepath later on.
*/
DraggableObject::DraggableObject() {}

/*
* Deconstructor for DraggableObject. Removes all events we started.
*/
DraggableObject::~DraggableObject() {
    EventHandler* event_handler = EventHandler::get_global_event_handler();
    event_handler->remove_hooks(get_event_name());
}

/*
* Watches the parent as well as traverses through each of
* the children <traverse_num> of times.
*
* This function will clear previously watched nodes.
*/
void DraggableObject::watch_node_path(NodePath& parent, int traverse_num) {
    _nodes.clear();
    _selected.clear();
    _parent = parent;
    _traverse_num = traverse_num;
    traverse();
}

/*
* Returns boolean indicating if the given np is managed by this draggable.
* This will only ever return true if DraggableObject was initialized via
* DraggableObject(parent, traverse_num) or if watch_node_path was called.
*/
bool DraggableObject::has_node(NodePath& np) {
    for (NodePath& other : _nodes) {
        // Other means of np == other:
        if (np.get_pos() != other.get_pos()) {
            continue;
        }
        if (np.get_name() != other.get_name()) {
            continue;
        }
        if (np.get_parent().get_name() != other.get_parent().get_name()) {
            continue;
        }
        return true;
    }
    return false;
}

/*
* Only called when DraggableObject is given a parent and a traverse num.
* Keeps an internal list of  the children to make matching easier for the DOM.
*/
void DraggableObject::traverse() {
     // If traverse_num is 0, we just want the parent.
    if (_traverse_num == 0) {
        _nodes.push_back(_parent);
        return;
    }

    // Otherwise, we need to iterate through everyone traverse_num times.
    traverse_children(_parent.get_children(), 1);
}

/*
* Recursively traverses children and pushes to _nodes vector.
* count indicates the current traversal cycle of a single node.
* Stops for each child when count == the given traverse num from init. 
*/
void DraggableObject::traverse_children(NodePathCollection& children, int count) {
    // Iterate through children.
    for (size_t i = 0; i < children.get_num_paths(); i++) {
        // Push child to vector.
        _nodes.push_back(children.get_path(i));

        // Determine if we have reached our limit.
        if (count == _traverse_num) {
            continue;
        }

        // Otherwise, keep going.
        traverse_children(children.get_path(i).get_children(), count + 1);
    }
}

/*
* Returns vector of NodePaths representing the selected objects.
*/
pvector<NodePath> DraggableObject::get_selected() {
    return _selected;
}

/*
* Object has been selected. Children may inherit for additional functionality.
*/
void DraggableObject::select(NodePath& np) {
    np.set_color(0, 1, 0, 1);
    _selected.push_back(np);
}

/*
* Object has been deselected. Children may inherit for additional functionality.
*/
void DraggableObject::deselect(NodePath& np) {
    np.clear_color();

    // Get index of np in _selected:
    pvector<NodePath>::iterator it = std::find(_selected.begin(), _selected.end(), np);
    if (it == _selected.end()) {
        return;
    }

    int index = std::distance(_selected.begin(), it);

    // Remove from vector:
    _selected.erase(_selected.begin() + index);
}

/*
* Object and all managed children has been deselected. Internally calls
* deselect(NodePath) for each node in the registered nodes vector.
* 
* If DraggableObject was initialized via a tag, traverses the scenegraph at the parent
* and calls deselect(NodePath) on all nodes that had the tag.
*/
void DraggableObject::deselect() {
    // Where DraggableObject(NodePath)
    size_t o_size = _selected.size();
    for (size_t i = 0; i < o_size; i++) {
        deselect(_selected[0]);
    }
}

/*
* Accepts the given event name and a callback function.
* While semi-equivalent to base.accept(<event>, <function>), this function
* will also register the event with the DraggableObjectManager.
*/
void DraggableObject::hook_drag_event(std::string event_name, EventHandler::EventCallbackFunction function, void* args) {
    EventHandler *event_handler = EventHandler::get_global_event_handler();
    DraggableObjectManager* dom = DraggableObjectManager::get_global_ptr();
    event_handler->add_hook(event_name, function, args);
    dom->register_event(*this, event_name);
    _event_name = event_name;
}

/*
* Returns the event name sent by hook_drag_event.
*/
std::string DraggableObject::get_event_name() const {
    return _event_name;
}

/*
* Returns boolean representing if we were set via DraggableObject(string tag) or not.
* i.e. if tag is "".
*/
bool DraggableObject::is_watching_tag() const {
    return _tag != "";
}

/*
* Returns tag set by DraggableObject(string tag).
*/
std::string DraggableObject::get_tag() const {
    return _tag;
}

/*
* Returns boolean if anything has been selected from this object.
*/
bool DraggableObject::has_selected() const {
    return _selected.size() > 0;
}
