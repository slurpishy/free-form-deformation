#include "freeFormDeform.h"

FreeFormDeform::FreeFormDeform(NodePath np, NodePath render) {
    _np = np;
    _render = render;

    _lattice = new Lattice(_np);
    _lattice->reparent_to(_render);

    process_node();
}

FreeFormDeform::~FreeFormDeform() {

    if (_task_mgr->find_task("DragWatcherTask") != nullptr) {
        _task_mgr->remove(_clicker_task);
    }

    // Ignore mouse1:
    EventHandler* event_handler = EventHandler::get_global_event_handler();
    event_handler->remove_hooks_with((void*)_c_args);

    // Delete the Lattice:
    delete _lattice;

    // Collision / Clicking Data:
    _picker_node.remove_node();
    delete _c_args;
    delete _traverser;
}

double FreeFormDeform::factorial(double n) {
    if (n == 0) {
        return 1;
    }
    return n * factorial(n - 1);
}

int FreeFormDeform::binomial_coeff(int n, int k) {
    return factorial(n) / (factorial(k) * factorial(n - k));
}

double FreeFormDeform::bernstein(double v, double n, double x) {
    return binomial_coeff(n, v) * pow(x, v) * pow(1 - x, n - v);
}

/*
Returns the actual index of the control point given i, j, k.
*/
int FreeFormDeform::get_point_index(int i, int j, int k) {
    pvector<int> spans = _lattice->get_edge_spans();
    return i * (spans[2] + 1) * (spans[2] + 1) + j * (spans[1] + 1) + k;
}

/*
Returns i, j, k given the index.
*/
std::vector<int> FreeFormDeform::get_ijk(int index) {
    pvector<int> spans = _lattice->get_edge_spans();
    int l = spans[0];
    int m = spans[1];
    int n = spans[2];

    int i = (index / ((n + 1) * (m + 1))) % (l + 1);
    int j = (index / (n + 1)) % (m + 1);
    int k = (index % (n + 1));

    std::vector<int> ijk = { i, j, k };
    return ijk;
}

/*
Returns true/false if the given control point influences the given vertex (stu).
*/
bool FreeFormDeform::is_influenced(int index, double s, double t, double u) {
    pvector<int> spans = _lattice->get_edge_spans();
    NodePath &point_np = _lattice->get_control_point(index);

    std::vector<int> ijk = get_ijk(index);
    return bernstein(ijk[0], spans[0], s) * bernstein(ijk[1], spans[1], t) * bernstein(ijk[2], spans[2], u);
}

void FreeFormDeform::transform_vertex(GeomVertexData* data, GeomNode* geom_node, int index) {
    GeomVertexWriter rewriter(data, "vertex");

    LPoint3f default_vertex, default_object_space; // (stu)
    LVector3f x_ffd;

    pvector<int> influenced_arrays = _influenced_vertices[geom_node][index];
    pvector<LPoint3f> default_vertex_pos;

    int vertex = 0;
    for (int i = 0; i < influenced_arrays.size(); i++) {
        vertex = influenced_arrays[i];
        default_vertex_pos = _default_vertex_ws_os[geom_node][vertex];

        rewriter.set_row(vertex);

        default_vertex = default_vertex_pos[0];
        double s = default_vertex_pos[1][0];
        double t = default_vertex_pos[1][1];
        double u = default_vertex_pos[1][2];

        x_ffd = deform_vertex(s, t, u);
        rewriter.set_data3f(x_ffd);
    }
}

LVector3f FreeFormDeform::deform_vertex(double s, double t, double u) {
    pvector<int> spans = _lattice->get_edge_spans();

    double bernstein_coeff;
    int p_index = 0;

    LVector3f vec_i = LVector3f(0);
    for (int i = 0; i <= spans[0]; i++) {
        LVector3f vec_j = LVector3f(0);
        for (int j = 0; j <= spans[1]; j++) {
            LVector3f vec_k = LVector3f(0);
            for (int k = 0; k <= spans[2]; k++) {
                bernstein_coeff = bernstein(k, spans[2], u);

                LPoint3f p_ijk = _lattice->get_control_point_pos(p_index, _render);
                vec_k += bernstein_coeff * p_ijk;

                p_index++;
            }
            bernstein_coeff = bernstein(j, spans[1], t);
            vec_j += bernstein_coeff * vec_k;
        }
        bernstein_coeff = bernstein(i, spans[0], s);
        vec_i += bernstein_coeff * vec_j;
    }

    return vec_i;
}

void FreeFormDeform::update_vertices() {
    PT(GeomVertexData) vertex_data;
    PT(Geom) geom;

    for (GeomNode* geom_node : _geom_nodes) {
        for (size_t i = 0; i < geom_node->get_num_geoms(); i++) {
            geom = geom_node->modify_geom(i);
            vertex_data = geom->modify_vertex_data();
            for (size_t j = 0; j < _selected_points.size(); j++) {
                transform_vertex(vertex_data, geom_node, _selected_points[j]);
            }
            geom->set_vertex_data(vertex_data);
            geom_node->set_geom(i, geom);
        }
    }

    // Also updates the lattice:
    for (size_t i : _selected_points) {
        _lattice->update_edges(i);
    }
}

/*
*/
void FreeFormDeform::process_node() {
    NodePathCollection collection = _np.find_all_matches("**/+GeomNode");

    // Ignore if there's nothing.
    if (collection.get_num_paths() == 0) {
        return;
    }

    // Begin by caculating stu based on our bounding box.
    _lattice->calculate_lattice_vec();

    pvector<LVector3f> lattice_vec = _lattice->get_lattice_vecs();
    LVector3f S = lattice_vec[0];
    LVector3f T = lattice_vec[1];
    LVector3f U = lattice_vec[2];

    GeomVertexReader v_reader;
    LPoint3f vertex, vertex_minus_min;

    pvector<LPoint3f> vertex_object_space;

    CPT(GeomVertexData) vertex_data;
    PT(GeomNode) geom_node;
    CPT(Geom) geom;

    bool influenced = false;

    int row = 0;

    LVector3f x0 = _lattice->get_x0();
    LVector3f x1 = _lattice->get_x1();

    for (size_t i = 0; i < collection.get_num_paths(); i++) {
        geom_node = DCAST(GeomNode, collection.get_path(i).node());
        for (size_t j = 0; j < geom_node->get_num_geoms(); j++) {
            geom = geom_node->get_geom(j);
            vertex_data = geom->get_vertex_data();

            // Store our unmodified points:
            v_reader = GeomVertexReader(vertex_data, "vertex");
            row = 0;
            while (!v_reader.is_at_end()) {
                vertex_object_space.clear();
                vertex = v_reader.get_data3f();

                vertex_minus_min = vertex - _lattice->get_x0();

                double s = (T.cross(U).dot(vertex_minus_min)) / T.cross(U).dot(S);
                double t = (S.cross(U).dot(vertex_minus_min)) / S.cross(U).dot(T);
                double u = (S.cross(T).dot(vertex_minus_min)) / S.cross(T).dot(U);

                vertex_object_space.push_back(vertex);
                vertex_object_space.push_back(LPoint3f(s, t, u));
                _default_vertex_ws_os[geom_node].push_back(vertex_object_space);

                // We do not care about vertices that aren't within our lattice.
                if (!_lattice->point_in_range(vertex)) {
                    row++;
                    continue;
                }

                // We're going to determine if this vertex is modified by a control point.
                for (size_t ctrl_i = 0; ctrl_i < _lattice->get_num_control_points(); ctrl_i++) {
                    influenced = is_influenced(ctrl_i, s, t, u);
                    if (influenced) {
                        _influenced_vertices[geom_node][ctrl_i].push_back(row);
                    }
                }
                row++;
            }
        }
        _geom_nodes.push_back(geom_node);
    }
}

void FreeFormDeform::set_edge_spans(int size_x, int size_y, int size_z) {
    _lattice->set_edge_spans(size_x, size_y, size_y);
}

LPoint3f FreeFormDeform::point_at_axis(double axis_value, LPoint3f point, LVector3f vector, int axis) {
    return point + vector * ((axis_value - point[axis]) / vector[axis]);
}

/*
Updates the vertices whenever there is a point selected.
Uses the pre-existing control point position.
*/
AsyncTask::DoneStatus FreeFormDeform::drag_task(GenericAsyncTask* task, void* args) {
    ClickerArgs* c_args = (ClickerArgs*)args;
   
    PT(MouseWatcher) mouse = c_args->mouse;
    FreeFormDeform* ffd = c_args->ffd;

    // Ignore if mouse is out of bounds:
    if (!mouse->has_mouse()) {
        return AsyncTask::DS_again;
    }
    
    // Ignore if there's nothing selected.
    if (ffd->_selected_points.size() == 0) {
        ffd->_object_handles->set_active(false);
        ffd->_object_handles->clear_node_paths();
        return AsyncTask::DS_again;
    }

    ffd->_object_handles->set_active(true);
    ffd->update_vertices();
    return AsyncTask::DS_cont;
}

void FreeFormDeform::handle_click(const Event* event, void* args) {
    ClickerArgs* c_args = (ClickerArgs*)args;

    PT(MouseWatcher) mouse = c_args->mouse;
    FreeFormDeform* ffd = c_args->ffd;
    WindowFramework* window = c_args->window;

    if (!mouse->has_mouse()) {
        return;
    }

    PT(Camera) camera = window->get_camera(0);
    ffd->_collision_ray->set_from_lens(camera, mouse->get_mouse().get_x(), mouse->get_mouse().get_y());

    NodePath render = window->get_render();
    ffd->_traverser->traverse(render);
    ffd->_handler_queue->sort_entries();

    NodePath control_point;
    if (ffd->_handler_queue->get_num_entries() == 0) {
        for (int i : ffd->_selected_points) {
            control_point = ffd->_lattice->get_control_point(i);
            control_point.set_color(1, 0, 1, 1);
        }
        ffd->_selected_points.clear();
        return;
    }

    NodePath into_node = ffd->_handler_queue->get_entry(0)->get_into_node_path();
    NodePath* into_node_ptr = &into_node;

    if (!into_node.has_net_tag("control_point")) {
        return;
    }

    int point_index = atoi(into_node.get_net_tag("control_point").c_str());

    pvector<int>::iterator it;
    it = std::find(ffd->_selected_points.begin(), ffd->_selected_points.end(), point_index);

    size_t i;
    if (it != ffd->_selected_points.end()) {
        i = std::distance(ffd->_selected_points.begin(), it);

        // Deselect:
        ffd->_selected_points.erase(ffd->_selected_points.begin() + i);
        into_node.clear_color();
        return;
    }

    // Select:
    ffd->_selected_points.push_back(point_index);

    control_point = ffd->_lattice->get_control_point(point_index);

    control_point.set_color(0, 1, 0, 1);

    // Need to actually get the proper NodePath for this:
    ffd->_object_handles->add_node_path(control_point);
}

void FreeFormDeform::setup_clicker(WindowFramework &window) {
   _traverser = new CollisionTraverser("FFD_Traverser");

    PT(CollisionNode) collision_node = new CollisionNode("mouse_ray");
    collision_node->set_from_collide_mask(GeomNode::get_default_collide_mask());

    _picker_node = window.get_camera_group().attach_new_node(collision_node);

    _collision_ray = new CollisionRay();
    collision_node->add_solid(_collision_ray);

    _handler_queue = new CollisionHandlerQueue();
    _traverser->add_collider(_picker_node, _handler_queue);

    NodePath mouse = window.get_mouse();
    PT(MouseWatcher) mouse_ptr = DCAST(MouseWatcher, mouse.node());

    // Click mouse1 Event:
    _c_args = new ClickerArgs{ mouse_ptr, &window, this };

    EventHandler *event_handler = EventHandler::get_global_event_handler();
    event_handler->add_hook("shift-mouse1", handle_click, _c_args);

    // Dragging Task:
    _clicker_task = new GenericAsyncTask("DragWatcherTask", &drag_task, _c_args);
    _task_mgr->add(_clicker_task);

    // Object Handles:
    _object_handles = new ObjectHandles(NodePath(), window.get_mouse(), window.get_camera_group(), window.get_camera(0));
}
