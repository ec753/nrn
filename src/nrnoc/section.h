/* /local/src/master/nrn/src/nrnoc/section.h,v 1.4 1996/05/21 17:09:24 hines Exp */
#pragma once

/* In order to support oc objects containing sections, instead of vector
    of ordered sections, we now have a list (in the nmodl sense)
    of unordered sections. The lesser efficiency is ok because the
    number crunching is vectorized. ie only the user interface deals
    with sections and that needs to be convenient
*/
/* Data structure for solving branching 1-D tree diffusion type equations.
    Vector of ordered sections each of which points to a vector of nodes.
    Each section must have at least one node. There may be 0 sections.
    The order of last node to first node is used in triangularization.
    First node to last is used in back substitution.
    The first node of a section is connected to some node of a section
      with lesser index.
*/
/* An equation is associated with each node. d and rhs are the diagonal and
   right hand side respectively.  a is the effect of this node on the parent
   node's equation.  b is the effect of the parent node on this node's
   equation.
   d is assumed to be non-zero.
   d and rhs is calculated from the property list.
*/

#include "membfunc.h"
#include "neuron/container/mechanism.hpp"
#include "neuron/container/node.hpp"

#include "nrnredef.h"
#include "options.h"
#include "hoclist.h"

/*#define DEBUGSOLVE 1*/
#define xpop      hoc_xpop
#define pc        hoc_pc
#define spop      hoc_spop
#define execerror hoc_execerror
#include "hocdec.h"

#include <optional>

#if DIAMLIST
struct Pt3d {
    float x, y, z, d;  // 3d point, microns
    double arc;
};
#endif
struct Section {
    int refcount{};        // may be in more than one list
    short nnode{};         // Number of nodes for ith section
    Section* parentsec{};  // parent section of node 0
    Section* child{};      // root of the list of children connected to this parent kept in order of
                           // increasing x
    Section* sibling{};    // used as list of sections that have same parent

    Node* parentnode{};     // parent node; only valid when tree_changed = 0
    Node** pnode{};         // Pointer to pointer vector of node structures
    int order{};            // index of this in secorder vector
    short recalc_area_{};   // NODEAREA, NODERINV, diam, L need recalculation
    short volatile_mark{};  // for searching
    void* volatile_ptr{};   // e.g. ShapeSection*
#if DIAMLIST
    short npt3d{};               // number of 3-d points
    short pt3d_bsize{};          // amount of allocated space for 3-d points
    Pt3d* pt3d{};                // list of 3d points with diameter
    Pt3d* logical_connection{};  // nil for legacy, otherwise specifies logical connection position
                                 // (for translation)
#endif
    Prop* prop{};  // eg. length, etc.
};

typedef float NodeCoef;
typedef double NodeVal;

typedef struct Info3Coef {
    NodeVal current; /* for use in next time step */
    NodeVal djdv0;
    NodeCoef coef0;    /* 5dx/12 */
    NodeCoef coefn;    /* 1dx/12 */
    NodeCoef coefjdot; /* dx^2*ra/12 */
    NodeCoef coefdg;   /* dx/12 */
    NodeCoef coefj;    /* 1/(ra*dx) */
    struct Node* nd2;  /* the node dx away in the opposite direction*/
    /* note above implies that nodes next to branches cannot have point processes
    and still be third order correct. Also nodes next to branches cannot themselves
    be branch points */
} Info3Coef;

typedef struct Info3Val { /* storage to help build matrix efficiently */
    NodeVal GC;           /* doesn't include point processes */
    NodeVal EC;
    NodeCoef Cdt;
} Info3Val;


/* if any double is added after area then think about changing
the notify_free_val parameter in node_free in solve.cpp
*/

#define NODED(n)   (*((n)->_d))
#define NODERHS(n) (*((n)->_rhs))

#undef NODEV /* sparc-sun-solaris2.9 */

#define NODEV(n)    ((n)->v_hack())
#define NODEAREA(n) ((n)->area())
#define NODERINV(n) ((n)->_rinv)
// The VEC_* vectors access the underlying array storage, i.e. the vectors that
// live inside structures like neuron::model().node_data().
// TODO: note that the new draft has one set of underlying storage vectors for
// all threads, while the old version had separate vectors for each NrnThread
#define VEC_A(i)    (_nt->_actual_a[(i)])
#define VEC_B(i)    (_nt->_actual_b[(i)])
#define VEC_D(i)    (_nt->_actual_d[(i)])
#define VEC_RHS(i)  (_nt->_actual_rhs[(i)])
#define VEC_V(i)    (_nt->actual_v(i))
#define VEC_AREA(i) (_nt->actual_area(i))
#define NODEA(n)    (VEC_A((n)->v_node_index))
#define NODEB(n)    (VEC_B((n)->v_node_index))

extern int use_sparse13;
extern int use_cachevec;
extern int secondorder;
extern int cvode_active_;

struct Extnode;
struct Node {
    // Eventually the old Node class should become an alias for
    // neuron::container::handle::Node, but as an intermediate measure we can
    // add one of those as a member and forward some access/modifications to it.
    neuron::container::Node::owning_handle _node_handle{neuron::model().node_data()};
    [[nodiscard]] auto& area() {
        return _node_handle.area_hack();
    }
    [[nodiscard]] auto const& area() const {
        return _node_handle.area_hack();
    }
    [[nodiscard]] auto area_handle() {
        return _node_handle.area_handle();
    }
    void set_area(neuron::container::Node::field::Area::type area) {
        _node_handle.set_area(area);
    }
    [[nodiscard]] auto& v() {
        return _node_handle.v_hack();
    }
    [[nodiscard]] auto const& v() const {
        return _node_handle.v_hack();
    }
    [[nodiscard]] auto& v_hack() {
        return _node_handle.v_hack();
    }
    [[nodiscard]] auto const& v_hack() const {
        return _node_handle.v_hack();
    }
    [[nodiscard]] auto v_handle() {
        return _node_handle.v_handle();
    }
    void set_v(neuron::container::Node::field::Voltage::type v) {
        _node_handle.set_v(v);
    }
    double _rinv{}; /* conductance uS from node to parent */
    double* _d;     /* diagonal element in node equation */
    double* _rhs;   /* right hand side in node equation */
    double* _a_matelm;
    double* _b_matelm;
    int eqn_index_;                 /* sparse13 matrix row/col index */
                                    /* if no extnodes then = v_node_index +1*/
                                    /* each extnode adds nlayer more equations after this */
    Prop* prop{};                   /* Points to beginning of property list */
    Section* child;                 /* section connected to this node */
                                    /* 0 means no other section connected */
    Section* sec;                   /* section this node is in */
                                    /* #if PARANEURON */
    struct Node* _classical_parent; /* needed for multisplit */
    struct NrnThread* _nt;
/* #endif */
#if EXTRACELLULAR
    Extnode* extnode{};
#endif

#if EXTRAEQN
    Eqnblock* eqnblock{}; /* hook to other equations which
           need to be solved at the same time as the membrane
           potential. eg. fast changeing ionic concentrations */
#endif

#if DEBUGSOLVE
    double savd;
    double savrhs;
#endif                   /*DEBUGSOLVE*/
    int v_node_index;    /* only used to calculate parent_node_indices*/
    int sec_node_index_; /* to calculate segment index from *Node */
    Node() = default;
    Node(Node const&) = delete;
    Node(Node&&) = default;
    Node& operator=(Node const&) = delete;
    Node& operator=(Node&&) = default;
    ~Node();
    friend std::ostream& operator<<(std::ostream& os, Node const& n) {
        return os << n._node_handle;
    }
};

#if EXTRACELLULAR
/* pruned to only work with sparse13 */
extern int nrn_nlayer_extracellular;
#define nlayer (nrn_nlayer_extracellular) /* first (0) layer is extracellular next to membrane */
struct Extnode {
    std::vector<neuron::container::data_handle<double>> param{};
    // double* param; /* points to extracellular parameter vector */
    /* v is membrane potential. so v internal = Node.v + Node.vext[0] */
    /* However, the Node equation is for v internal. */
    /* This is reconciled during update. */

    /* Following all have allocated size of nlayer */
    double* v; /* v external. */
    double* _a;
    double* _b;
    double** _d;
    double** _rhs; /* d, rhs, a, and b are analogous to those in node */
    double** _a_matelm;
    double** _b_matelm;
    double** _x12; /* effect of v[layer] on eqn layer-1 (or internal)*/
    double** _x21; /* effect of v[layer-1 or internal] on eqn layer*/
};
#endif

#if !INCLUDEHOCH
#include "hocdec.h" /* Prop needs Datum and Datum needs Symbol */
#endif

#define PROP_PY_INDEX 10

struct Prop {
    // Working assumption is that we can safely equate "Prop" with "instance
    // of a mechanism" apart from a few special cases like CABLESECTION
    Prop(short type)
        : _type{type} {
        if (type != CABLESECTION) {
            m_mech_handle = neuron::container::Mechanism::owning_handle{
                neuron::model().mechanism_data(type)};
        }
    }
    Prop* next;      /* linked list of properties */
    short _type;     /* type of membrane, e.g. passive, HH, etc. */
    short unused1;   /* gcc and borland need pairs of shorts to align the same.*/
    int dparam_size; /* for notifying hoc_free_val_array */
    // double* param;     /* vector of doubles for this property */
    Datum* dparam;   /* usually vector of pointers to doubles
                of other properties but maybe other things as well
                for example one cable section property is a
                symbol */
    long _alloc_seq; /* for cache efficiency */
    Object* ob;      /* nil if normal property, otherwise the object containing the data*/

    /** @brief Get the identifier of this instance.
     */
    [[nodiscard]] auto id() const {
        assert(m_mech_handle);
        return m_mech_handle->id_hack();
    }

    /** @brief Check if the given handle refers to data owned by this Prop.
     */
    [[nodiscard]] bool owns(neuron::container::data_handle<double> const& handle) const {
        auto const num_fpfields = param_size();
        auto* const raw_ptr = static_cast<double const*>(handle);
        for (auto i = 0; i < num_fpfields; ++i) {
            if (raw_ptr == &m_mech_handle->fpfield(i)) {
                return true;
            }
        }
        return false;
    }

    /** @brief Return a reference to the i-th double value associated with this Prop.
     */
    [[nodiscard]] double& param(std::size_t i) {
        assert(m_mech_handle);
        return m_mech_handle->fpfield(i);
    }

    /** @brief Return a reference to the i-th double value associated with this Prop.
     */
    [[nodiscard]] double const& param(std::size_t i) const {
        assert(m_mech_handle);
        return m_mech_handle->fpfield(i);
    }

    /** @brief Return a handle to the i-th double value associated with this Prop.
     */
    [[nodiscard]] auto param_handle(std::size_t i) {
        assert(m_mech_handle);
        return m_mech_handle->fpfield_handle(i);
    }

    /** @brief Return how many double values are assocated with this Prop.
     */
    [[nodiscard]] std::size_t param_size() const {
        assert(m_mech_handle);
        return m_mech_handle->num_fpfields();
    }

    /** @brief Update the i-th double value associated with this Prop.
     */
    void set_param(std::size_t i, double value) {
        assert(m_mech_handle);
        m_mech_handle->set_fpfield(i, value);
    }

    friend std::ostream& operator<<(std::ostream& os, Prop const& p) {
        if (p.m_mech_handle) {
            return os << *p.m_mech_handle;
        } else {
            return os << "Prop{nullopt}";
        }
    }

  private:
    // This is a handle that owns a row of the ~global mechanism data for
    // `_type`. Usage of `param` and `param_size` should be replaced with
    // indirection through this.
    std::optional<neuron::container::Mechanism::owning_handle> m_mech_handle;
};

/** @brief Helper to generate an _ml, _iml pair from Prop*.
 *
 *  In generated code then things like range variables are preprocessor macros
 *  that expand to things like _ml->data(_iml, 42). If one wants to call
 *  something using these variables for a single mechanism instance (single
 *  Prop) then one needs to create an appropriate _ml and _iml pair so the
 *  macros expand to something that compiles. Guaranteed copy elision in C++17
 *  should guarantee that the Memb_list* is valid in the calling scope.
 */
inline std::tuple<Memb_list, Memb_list*, std::size_t> create_ml(Prop* p) {
    std::tuple<Memb_list, Memb_list*, std::size_t> ret{};
    if (p) {
        std::get<0>(ret) = {p->_type};
        std::get<0>(ret).set_storage_offset(p->id().current_row());
        std::get<1>(ret) = &std::get<0>(ret);
    }
    return ret;
}

namespace neuron::legacy {
/**
 * @brief Helper for legacy MOD files that mess with _p in VERBATIM blocks.
 */
inline void set_globals_from_prop(Prop* p, Memb_list& ml, Memb_list*& ml_ptr, std::size_t& iml) {
    if (!p) {
        return;
    }
    ml = {p->_type};
    ml.set_storage_offset(p->id().current_row());
    ml_ptr = &ml;
    iml = 0;
}
}  // namespace neuron::legacy

extern Datum* nrn_prop_datum_alloc(int type, int count, Prop* p);
extern void nrn_prop_datum_free(int type, Datum* ppd);
extern double nrn_ghk(double, double, double, double);

/* a point process is computed just like regular mechanisms. Ie it appears
in the property list whose type specifies which allocation, current, and
state functions to call.  This means some nodes have more properties than
other nodes even in the same section.  The Point_process structure allows
the interface to hoc variable names.
Each variable symbol u.rng->type refers to the point process mechanism.
The variable is treated as a vector
variable whose first index specifies "which one" of that mechanisms insertion
points we are talking about.  Finally the variable u.rng->index tells us
where in the p-array to look.  The number of point_process vectors is the
number of different point process types.  This is different from the
mechanism type which enumerates all mechanisms including the point_processes.
It is the responsibility of create_point_process to set up the vectors and
fill in the symbol information.  However only after the process is given
a location can the variables be set or accessed. This is because the
allocation function may have to connect to some ionic parameters and the
process exists primarily as a property of a node.
*/
struct Point_process {
    Section* sec{}; /* section and node location for the point mechanism*/
    Node* node{};
    Prop* prop{};    /* pointer to the actual property linked to the
                  node property list */
    Object* ob{};    /* object that owns this process */
    void* presyn_{}; /* non-threshold presynapse for NetCon */
    void* nvi_{};    /* NrnVarIntegrator (for local step method) */
    void* _vnt{};    /* NrnThread* (for NET_RECEIVE and multicore) */
};

#if EXTRAEQN
/*Blocks of equations can hang off each node of the current conservation
equations. These are equations which must be solved simultaneously
because they depend on the voltage and affect the voltage. An example
are fast changing ionic concentrations (or merely if we want to be
able to calculate steady states using a stable method).
*/
typedef struct Eqnblock {
    struct Eqnblock* eqnblock_next; /* may be several such blocks */
    Pfri eqnblock_triang;           /* triangularization function */
    Pfri eqnblock_bksub;            /* back substitution function */
    double* eqnblock_data;
#if 0
	the solving functions know how to find the following info from
	the eqnblock_data.
	double *eqnblock_row;	/* current conservation depends on states */
	double *eqnblock_col;	/* state equations depend on voltage */
	double *eqnblock_matrix; /* state equations depend on states */
	double *eqnblock_rhs:
	the functions merely take a pointer to the node and this Eqnblock
	in order to update the values of the diagonal, v, and the rhs
	The interface with EXTRACELLULAR makes things a bit more subtle.
	It seems clear that we will have to change the meaning of v to
	be membrane potential so that the internal potential is v + vext.
	This will avoid requiring two rows and two columns since
	the state equations will depend only on v and not vext.
	In fact, if vext did not have longitudinal relationships with
	other vext the extracellular mechanism could be implemented in
	this style.
#endif
} Eqnblock;
#endif /*EXTRAEQN*/

extern int nrn_global_ncell;   /* note that for multiple threads all the rootnodes are no longer
                                  contiguous */
extern hoc_List* section_list; /* Where the Sections live */

extern Section* sec_alloc();             /* Allocates a single section */
extern void node_alloc(Section*, short); /* Allocates node vectors in a section*/
extern double section_length(Section*), nrn_diameter(Node*);
extern Node* nrn_parent_node(Node*);
extern Section* nrn_section_alloc();
extern void nrn_section_free(Section*);
extern int nrn_is_valid_section_ptr(void*);


#include <multicore.h>

#define tstopbit   (1 << 15)
#define tstopset   stoprun |= tstopbit
#define tstopunset stoprun &= (~tstopbit)
/* cvode.event(tevent) sets this. Reset at beginning */
/* of any hoc call for integration and before returning to hoc */


#include "nrn_ansi.h"
