#ifndef OBJECTHANDLES_H
#define OBJECTHANDLES_H

#include "nodePath.h"
#include "genericAsyncTask.h"
#include "mouseWatcher.h"
#include "lineSegs.h"
#include "asyncTaskManager.h"
#include "camera.h"
#include "trackball.h"
#include "transform2sg.h"
#include "mouseButton.h"
#include "plane.h"
#include "cardMaker.h"
#include "planeNode.h"
#include "loader.h"
#include "boundingSphere.h"

class ObjectHandles : public NodePath {
public:
    ObjectHandles(NodePath &np, NodePath _mouse_np, NodePath camera_np, Camera *camera);
    ~ObjectHandles();

    void cleanup();

    inline void set_length(double length);
    inline double get_length() const;

    inline void set_thickness(double thickness);
    inline double get_thickness() const;

    inline void set_active(bool active);
    inline bool is_active() const;

    void add_node_path(NodePath &np);
    void remove_node_path(NodePath& np);
    void clear_node_paths();

private:
    enum AxisType {
        AT_x,
        AT_y,
        AT_z,
        AT_xy,
        AT_xz,
        AT_yz,
        AT_all,
    };

private:
    static AsyncTask::DoneStatus mouse_task(GenericAsyncTask* task, void* args);
    static AsyncTask::DoneStatus mouse_drag_task(GenericAsyncTask* task, void* args);
    static void handle_click(const Event* event, void* args);
    static void handle_drag_done(const Event* event, void* args);

    LPoint2f convert_to_2d_space(NodePath& np, LPoint3f& origin, LMatrix4& proj_mat);
    NodePath create_plane_np(LPoint3f pos, LPoint3f hpr, LColor color, AxisType axis_type);

    void rebuild();
    void setup_mouse_watcher();
    void disable_camera_movement();
    void enable_camera_movement();
    void update_scale(LMatrix4& proj_mat);

private:
    // Task Names:
    const std::string DRAG_TASK_NAME = "ObjectHandles_MouseDragTask";
    const std::string MOUSE_TASK_NAME = "ObjectHandles_MouseTask";

    double _length = 0.5;
    double _thickness = 3.0;
   
    const double _PLANE_AXIS_D = 0.07;

    bool _active;

    NodePath _np;
    NodePath _np_parent;
    NodePath _camera_np;
    NodePath _mouse_np;
    NodePath _trackball_np;
    NodePath _hover_line_np;
    NodePath _active_line_np;
    NodePath* _np_obj_node;
    NodePath* _control_node;

    PT(Trackball) _trackball;
    PT(MouseWatcher) _mouse_watcher;
    PT(Transform2SG) _trackball2Cam;
    PT(PandaNode) _trackball_node;
    PT(AsyncTask) _drag_task;
    PT(Camera) _camera;

    LMatrix4f _cam_mat;
    LPoint3f previous_pos3d;

    pvector<NodePath> _axis_nps;
    pvector<NodePath> _axis_plane_nps;

    pmap<NodePath, pvector<NodePath>> _nps;
};

#include "objectHandles.I"

#endif
