#include "draggableObjectManager.h"

DraggableObjectManager* DraggableObjectManager::_global_ptr = nullptr;

DraggableObjectManager::DraggableObjectManager() {}

/*
Initializes everything for the DraggableObjectManager.
parent (normally rendere) is what the CollisionTraverser
eventually calls traverse on every click.
*/
DraggableObjectManager::DraggableObjectManager(NodePath &parent, NodePath &camera_np, NodePath &mouse_np) {
    setup_nodes(parent, camera_np, mouse_np);
}


/*
Sets the parent, camera, and mouse and its underlying nodes.
*/
void DraggableObjectManager::setup_nodes(NodePath &parent, NodePath &camera_np, NodePath &mouse_np) {
    _parent = parent;
    _camera_np = camera_np;
    _camera = DCAST(Camera, _camera_np.node());
    _mouse_np = mouse_np;
    _mouse = DCAST(MouseWatcher, mouse_np.node());
    setup_picking_objects();
}

/*
Registers object with the manager.
*/
void DraggableObjectManager::register_object(DraggableObject& draggable) {
    _objects.push_back(draggable);

    // Check if this has a tag:
    if (draggable.is_watching_tag()) {
        // Keep track:
        _tag_map[draggable.get_tag()] = draggable;
    }
}

/*
Sets up the traverser, handler, and ray.
*/
void DraggableObjectManager::setup_picking_objects() {
    // Setup CollisionTraverser:
    _traverser = new CollisionTraverser("DOM_Traverser");

    // Collision Node for our ray picker
    PT(CollisionNode) collision_node = new CollisionNode("DOM_PickerNode");

    // Allow it to see all the GeomNodes.
    collision_node->set_from_collide_mask(GeomNode::get_default_collide_mask());

    // Attach to our camera:
    NodePath picker_node = _camera_np.attach_new_node(collision_node);

    // Setup our ray:
    _collision_ray = new CollisionRay();
    collision_node->add_solid(_collision_ray);

    // Then a handler queue:
    _handler_queue = new CollisionHandlerQueue();
    _traverser->add_collider(picker_node, _handler_queue);

    // also our object handles..
    object_handles = new ObjectHandles(
        NodePath(),
        _mouse_np,
        _camera_np,
        _camera
    );
}

/*

*/
void DraggableObjectManager::click() {
    _collision_ray->set_from_lens(
        _camera,
        _mouse->get_mouse_x(),
        _mouse->get_mouse_y()
    );

    // Traverse on whomever we received from init.
    _traverser->traverse(_parent);
    _handler_queue->sort_entries();

    // We got nothing.
    if (_handler_queue->get_num_entries() == 0) {
        // If the user clicked off, deselect everything
        deselect_all();
        return;
    }

    // Enable our object handles:
    object_handles->set_active(true);

    // Get our top entry:
    PT(CollisionEntry) entry = _handler_queue->get_entry(0);
    NodePath into_np = entry->get_into_node_path();

    DraggableObject draggable;

    // Check to see if we have any of special tags:
    for (std::map<std::string, DraggableObject>::iterator it = _tag_map.begin(); it != _tag_map.end(); it++) {
        if (into_np.has_net_tag(it->first)) {
            draggable = it->second;
            draggable.select(into_np);

            object_handles->add_node_path(into_np);
            return;
        }
    }
    
    // If we made is this far, that means that we didn't have a tag match.
    // Let's compare directly to our managed objects.
    // TODO
}


/*
*/
void DraggableObjectManager::deselect_all() {
    // Stop our handles:
    object_handles->clear_node_paths();
    object_handles->set_active(false);

    // Run deselect on all our managed nodes:
    for (DraggableObject& draggable : _objects) {
        draggable.deselect();
    }
}

/*
The function that gets called when the event assigned to
setup_mouse(x) is triggered.
*/
static void handle_mouse_click(const Event* e, void* args) {
    DraggableObjectManager* dom = (DraggableObjectManager*)args;

    // Ignore if there's no mouse.
    if (!dom->_mouse->has_mouse()) {
        return;
    }

    dom->click();
}

/*
Task that executes whenever we are dragging the mouse whilst the event
assigned to setup_mouse(x) is called.
*/
static AsyncTask::DoneStatus drag_task(GenericAsyncTask *task, void* args) {
    DraggableObjectManager* dom = (DraggableObjectManager*)args;
    return AsyncTask::DS_cont;
}

/*
Setups the bind and dragging task for the mouse.
*/
void DraggableObjectManager::setup_mouse(std::string click_button) {
    event_handler->add_hook(click_button, handle_mouse_click, this);

    // Setup our dragging task too:
    _clicker_task = new GenericAsyncTask("DOM_DragTask", &drag_task, this);
    task_mgr->add(_clicker_task);
}

/*

*/
DraggableObjectManager* DraggableObjectManager::get_global_ptr() {
    if (_global_ptr == nullptr) {
        _global_ptr = new DraggableObjectManager();
    }
    return _global_ptr;
}
