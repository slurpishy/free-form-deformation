#ifndef LATTICE_H
#define LATTICE_H

#include "geomVertexData.h"
#include "nodePath.h"
#include "loader.h"
#include "boundingSphere.h"
#include "lineSegs.h"
#include "textNode.h"
#include "draggableObject.h"
#include "draggableObjectManager.h"

#include <unordered_map>

class Lattice : public NodePath, public DraggableObject {
public:
    inline Lattice(NodePath np);
    inline ~Lattice();

    void create_control_points(const double radius);
    void calculate_lattice_vec();
    inline pvector<LVector3f> get_lattice_vecs() const;

    void update_edges(int index);
    void set_edge_spans(int size_x, int size_y, int size_z);
    inline std::vector<int>& get_edge_spans();

    void set_control_point_pos(LPoint3f pos, int index);
    inline NodePath& get_control_point(int index);
    inline LPoint3f &get_control_point_pos(int i, const NodePath& other);
    inline int get_num_control_points();
    inline std::vector<int>& get_selected_control_points();

    inline bool point_in_range(LPoint3f& point);

    inline LPoint3f get_x0() const;
    inline LPoint3f get_x1() const;

    inline std::vector<int>& get_ijk(int index);

    virtual void select(NodePath& np);
    virtual void deselect(NodePath& np);

    LPoint3f _edge_pos;
    NodePath _edgesNp;

    friend std::ostream& operator<<(std::ostream& os, Lattice& obj);

private:
    void create_point(LPoint3f point, const double radius, int i, int j, int k);
    void create_edges();
    void reset_control_points();
    void reset_edges();
    void rebuild();
    void push_point_edge(int index);
    void push_point_relationship(int index, int adjacent_index);

    Loader *_loader = Loader::get_global_ptr();
    DraggableObjectManager* _dom = DraggableObjectManager::get_global_ptr();

    pvector<NodePath> _control_points; // P(ijk)
    pvector<LVector3f> _lattice_vecs; // STU
    std::vector<int> _plane_spans = { 2, 3, 2 }; // lnm

    LPoint3f _x0, _x1;

    NodePath _np;

    LineSegs _edges;

    PT(GeometricBoundingVolume) _npBoundingLarge;

    // control point index -> line segment index
    pmap<int, pvector<int>> point_to_edge_vertex;

    // control point index -> control point index
    pmap<int, pvector<int>> point_map;

    pmap<int, pvector<int>> point_map_future;

    // control point index -> [i,j,k]
    std::unordered_map<int, std::vector<int>> _point_ijk_map;

    // Selected control points.
    std::vector<int> _selected_control_points;

    int num_segments = -1;
    
    bool initial_bounds_capture = false;
};

#include "lattice.I"

#endif
