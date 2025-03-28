#include "geomVertexData.h"
#include "nodePath.h"
#include "loader.h"

class Lattice : public NodePath {
public:
    Lattice(NodePath np);
    void create_control_points();
    void calculate_lattice_vec();

    void set_edge_spans(int size_x, int size_y, int size_z);
    pvector<int> get_edge_spans();

    pvector<LVector3f> get_lattice_vecs();
    LPoint3f get_control_point_pos(int i);
    LPoint3f get_x0();

private:
    void create_point(LPoint3f point);
    void reset_control_points();
    void rebuild();

    Loader *_loader = Loader::get_global_ptr();

    pvector<NodePath> _control_points; // P(ijk)
    pvector<LVector3f> _lattice_vecs; // STU
    pvector<int> _plane_spans = { 2, 3, 4 }; // lnm

    LPoint3f _x0, _x1;

    NodePath _np;
};
