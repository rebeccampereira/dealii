// ---------------------------------------------------------------------
//
// Copyright (C) 1998 - 2019 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------

#ifndef dealii_tria_h
#define dealii_tria_h


#include <deal.II/base/config.h>

#include <deal.II/base/geometry_info.h>
#include <deal.II/base/iterator_range.h>
#include <deal.II/base/point.h>
#include <deal.II/base/smartpointer.h>
#include <deal.II/base/subscriptor.h>

#include <deal.II/grid/tria_iterator_selector.h>

#include <boost/serialization/map.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/signals2.hpp>

#include <bitset>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <vector>


DEAL_II_NAMESPACE_OPEN

template <int dim, int spacedim>
class Manifold;

namespace GridTools
{
  template <typename CellIterator>
  struct PeriodicFacePair;
}

template <int, int, int>
class TriaAccessor;
template <int spacedim>
class TriaAccessor<0, 1, spacedim>;
template <int, int, int>
class TriaAccessorBase;

namespace internal
{
  namespace TriangulationImplementation
  {
    template <int dim>
    class TriaLevel;
    template <int dim>
    class TriaFaces;

    template <typename>
    class TriaObjects;

    /**
     * Forward declaration of a class into which we put much of the
     * implementation of the Triangulation class. See the .cc file for more
     * information.
     */
    struct Implementation;
  } // namespace TriangulationImplementation

  namespace TriaAccessorImplementation
  {
    struct Implementation;
  }
} // namespace internal

namespace hp
{
  template <int dim, int spacedim>
  class DoFHandler;
}


/*------------------------------------------------------------------------*/

/**
 * The CellData class (and the related SubCellData class) is used to
 * provide a comprehensive, but minimal, description of the cells when
 * creating a triangulation via Triangulation::create_triangulation().
 * Specifically, each CellData object -- describing one cell in a
 * triangulation -- has member variables for indices of the $2^d$ vertices
 * (the actual coordinates of the vertices are described in a separate
 * vector passed to Triangulation::create_triangulation(), so the CellData
 * object only needs to store indices into that vector), the material
 * id of the cell that can be used in applications to describe which
 * part of the domain a cell belongs to (see
 * @ref GlossMaterialId "the glossary entry on material ids"),
 * and a manifold id that is used to describe the geometry object
 * that is responsible for this cell (see
 * @ref GlossManifoldIndicator "the glossary entry on manifold ids")
 * to describe the manifold this object belongs to.
 *
 * This structure is also used to represent data for faces and edges when used
 * as a member of the SubCellData class. In this case, the template argument
 * @p structdim of an object will be less than the dimension @p dim of the
 * triangulation. If this is so, then #vertices array represents the indices of
 * the vertices of one face or edge of one of the cells passed to
 * Triangulation::create_triangulation(). Furthermore, for faces the
 * material id has no meaning, and the @p material_id field is reused
 * to store a @p boundary_id instead to designate which part of the boundary
 * the face or edge belongs to (see
 * @ref GlossBoundaryIndicator "the glossary entry on boundary ids").
 *
 * An example showing how this class can be used is in the
 * <code>create_coarse_grid()</code> function of step-14. There are also
 * many more use cases in the implementation of the functions of the
 * GridGenerator namespace.
 *
 * @ingroup grid
 */
template <int structdim>
struct CellData
{
  /**
   * Indices of the vertices of this cell. These indices correspond
   * to entries in the vector of vertex locations passed to
   * Triangulation::create_triangulation().
   */
  unsigned int vertices[GeometryInfo<structdim>::vertices_per_cell];

  /**
   * Material or boundary indicator of this cell.
   * This field is a union that stores <i>either</i> a boundary or
   * a material id, depending on whether the current object is used
   * to describe a cell (in a vector of CellData objects) or a
   * face or edge (as part of a SubCellData object).
   */
  union
  {
    /**
     * The material id of the cell being described. See the documentation
     * of the CellData class for examples of how to use this field.
     *
     * This variable can only be used if the current object is used to
     * describe a cell, i.e., if @p structdim equals the dimension
     * @p dim of a triangulation.
     */
    types::material_id material_id;

    /**
     * The boundary id of a face or edge being described. See the documentation
     * of the CellData class for examples of how to use this field.
     *
     * This variable can only be used if the current object is used to
     * describe a face or edge, i.e., if @p structdim is less than the dimension
     * @p dim of a triangulation. In this case, the CellData object this
     * variable belongs to will be part of a SubCellData object.
     */
    types::boundary_id boundary_id;
  };

  /**
   * Manifold identifier of this object. This identifier should be used to
   * identify the manifold to which this object belongs, and from which this
   * object will collect information on how to add points upon refinement.
   *
   * See the documentation of the CellData class for examples of how to use
   * this field.
   */
  types::manifold_id manifold_id;

  /**
   * Default constructor. Sets the member variables to the following values:
   *
   * - vertex indices to invalid values
   * - boundary or material id zero (the default for boundary or material ids)
   * - manifold id to numbers::flat_manifold_id
   */
  CellData();
};



/**
 * The SubCellData class is used to describe information about faces and
 * edges at the boundary of a mesh when creating a triangulation via
 * Triangulation::create_triangulation(). It contains member variables
 * that describe boundary edges and boundary quads.
 *
 * The class has no template argument and is used both in the description
 * of boundary edges in 2d (in which case the contents of the
 * @p boundary_quads member variable are ignored), as well as in the
 * description of boundary edges and faces in 3d (in which case both the
 * @p boundary_lines and @p boundary_quads members may be used). It is also
 * used as the argument to Triangulation::create_triangulation() in 1d,
 * where the contents of objects of the current type are simply ignored.
 *
 * By default, Triangulation::create_triangulation() simply assigns
 * default boundary indicators and manifold indicators to edges and
 * quads at the boundary of the mesh. (See the glossary entries on
 * @ref GlossBoundaryIndicator "boundary ids"
 * and
 * @ref GlossManifoldIndicator "manifold ids"
 * for more information on what they represent.) As a consequence,
 * it is not <i>necessary</i> to explicitly describe the properties
 * of boundary objects. In all cases, these properties can also be
 * set at a later time, once the triangulation has already been
 * created. On the other hand, it is sometimes convenient to describe
 * boundary indicators or manifold ids at the time of creation. In
 * these cases, the current class can be used by filling the
 * @p boundary_lines and @p boundary_quads vectors with
 * CellData<1> and CellData<2> objects that correspond to boundary
 * edges and quads for which properties other than the default
 * values should be used.
 *
 * Each entry in the @p boundary_lines and @p boundary_quads vectors
 * then needs to correspond to an edge or quad of the cells that
 * are described by the vector of CellData objects passed to
 * Triangulation::create_triangulation(). I.e., the vertex indices
 * stored in each entry need to correspond to an edge or face
 * of the triangulation that has the same set of vertex indices,
 * and in the same order. For these boundary edges or quads, one can
 * then set either or both the CellData::boundary_id and
 * CellData::manifold_id.
 *
 * There are also use cases where one may want to set the manifold id
 * of an <i>interior</i> edge or face. Such faces, identified by
 * their vertex indices, may also appear in the
 * @p boundary_lines and @p boundary_quads vectors (despite the names of
 * these member variables). However, it is then obviously not allowed
 * to set a boundary id (because the object is not actually part of
 * the boundary). As a consequence, to be valid, the CellData::boundary_id
 * of interior edges or faces needs to equal
 * numbers::internal_face_boundary_id.
 *
 * @ingroup grid
 */
struct SubCellData
{
  /**
   * A vector of CellData<1> objects that describe boundary and manifold
   * information for edges of 2d or 3d triangulations.
   *
   * This vector may not be used in the creation of 1d triangulations.
   */
  std::vector<CellData<1>> boundary_lines;

  /**
   * A vector of CellData<2> objects that describe boundary and manifold
   * information for quads of 3d triangulations.
   *
   * This vector may not be used in the creation of 1d or 2d triangulations.
   */
  std::vector<CellData<2>> boundary_quads;

  /**
   * Determine whether the member variables above which may not be used in a
   * given dimension are really empty. In other words, this function returns
   * whether
   * both @p boundary_lines and @p boundary_quads are empty vectors
   * when @p dim equals one, and whether the @p boundary_quads
   * vector is empty when @p dim equals two.
   */
  bool
  check_consistency(const unsigned int dim) const;
};


/*------------------------------------------------------------------------*/


namespace internal
{
  /**
   * A namespace for classes internal to the triangulation classes and
   * helpers.
   */
  namespace TriangulationImplementation
  {
    /**
     * Cache class used to store the number of used and active elements (lines
     * or quads etc) within the levels of a triangulation. This is only the
     * declaration of the template, concrete instantiations are below.
     *
     * In the old days, whenever one wanted to access one of these numbers,
     * one had to perform a loop over all lines, e.g., and count the elements
     * until we hit the end iterator. This is time consuming and since access
     * to the number of lines etc is a rather frequent operation, this was not
     * an optimal solution.
     *
     * @author Wolfgang Bangerth, 1999
     */
    template <int dim>
    struct NumberCache
    {};

    /**
     * Cache class used to store the number of used and active elements (lines
     * or quads etc) within the levels of a triangulation. This specialization
     * stores the numbers of lines.
     *
     * In the old days, whenever one wanted to access one of these numbers,
     * one had to perform a loop over all lines, e.g., and count the elements
     * until we hit the end iterator. This is time consuming and since access
     * to the number of lines etc is a rather frequent operation, this was not
     * an optimal solution.
     *
     * @author Wolfgang Bangerth, 1999
     */
    template <>
    struct NumberCache<1>
    {
      /**
       * The number of levels on which we have used objects.
       */
      unsigned int n_levels;

      /**
       * Number of used lines in the whole triangulation.
       */
      unsigned int n_lines;

      /**
       * Array holding the number of used lines on each level.
       */
      std::vector<unsigned int> n_lines_level;

      /**
       * Number of active lines in the whole triangulation.
       */
      unsigned int n_active_lines;

      /**
       * Array holding the number of active lines on each level.
       */
      std::vector<unsigned int> n_active_lines_level;

      /**
       * Constructor. Set values to zero by default.
       */
      NumberCache();

      /**
       * Determine an estimate for the memory consumption (in bytes) of this
       * object.
       */
      std::size_t
      memory_consumption() const;

      /**
       * Read or write the data of this object to or from a stream for the
       * purpose of serialization
       */
      template <class Archive>
      void
      serialize(Archive &ar, const unsigned int version);
    };


    /**
     * Cache class used to store the number of used and active elements (lines
     * or quads etc) within the levels of a triangulation. This specialization
     * stores the numbers of quads. Due to the inheritance from the base class
     * NumberCache<1>, the numbers of lines are also within this class.
     *
     * In the old days, whenever one wanted to access one of these numbers,
     * one had to perform a loop over all lines, e.g., and count the elements
     * until we hit the end iterator. This is time consuming and since access
     * to the number of lines etc is a rather frequent operation, this was not
     * an optimal solution.
     *
     * @author Wolfgang Bangerth, 1999
     */
    template <>
    struct NumberCache<2> : public NumberCache<1>
    {
      /**
       * Number of used quads in the whole triangulation.
       */
      unsigned int n_quads;

      /**
       * Array holding the number of used quads on each level.
       */
      std::vector<unsigned int> n_quads_level;

      /**
       * Number of active quads in the whole triangulation.
       */
      unsigned int n_active_quads;

      /**
       * Array holding the number of active quads on each level.
       */
      std::vector<unsigned int> n_active_quads_level;

      /**
       * Constructor. Set values to zero by default.
       */
      NumberCache();

      /**
       * Determine an estimate for the memory consumption (in bytes) of this
       * object.
       */
      std::size_t
      memory_consumption() const;

      /**
       * Read or write the data of this object to or from a stream for the
       * purpose of serialization
       */
      template <class Archive>
      void
      serialize(Archive &ar, const unsigned int version);
    };


    /**
     * Cache class used to store the number of used and active elements (lines
     * or quads etc) within the levels of a triangulation. This specialization
     * stores the numbers of hexes. Due to the inheritance from the base class
     * NumberCache<2>, the numbers of lines and quads are also within this
     * class.
     *
     * In the old days, whenever one wanted to access one of these numbers,
     * one had to perform a loop over all lines, e.g., and count the elements
     * until we hit the end . This is time consuming and since access to the
     * number of lines etc is a rather frequent operation, this was not an
     * optimal solution.
     *
     * @author Wolfgang Bangerth, 1999
     */
    template <>
    struct NumberCache<3> : public NumberCache<2>
    {
      /**
       * Number of used hexes in the whole triangulation.
       */
      unsigned int n_hexes;

      /**
       * Array holding the number of used hexes on each level.
       */
      std::vector<unsigned int> n_hexes_level;

      /**
       * Number of active hexes in the whole triangulation.
       */
      unsigned int n_active_hexes;

      /**
       * Array holding the number of active hexes on each level.
       */
      std::vector<unsigned int> n_active_hexes_level;

      /**
       * Constructor. Set values to zero by default.
       */
      NumberCache();

      /**
       * Determine an estimate for the memory consumption (in bytes) of this
       * object.
       */
      std::size_t
      memory_consumption() const;

      /**
       * Read or write the data of this object to or from a stream for the
       * purpose of serialization
       */
      template <class Archive>
      void
      serialize(Archive &ar, const unsigned int version);
    };
  } // namespace TriangulationImplementation
} // namespace internal


/*------------------------------------------------------------------------*/


/**
 * Triangulations denote a hierarchy of levels of elements which together form
 * a @p dim -dimensional manifold in @p spacedim spatial dimensions (if
 * spacedim is not specified it takes the default value @p spacedim=dim).
 *
 * Thus, for example, an object of type @p Triangulation<1,1> (or simply @p
 * Triangulation<1> since @p spacedim==dim by default) is used to represent
 * and handle the usual one-dimensional triangulation used in the finite
 * element method (so, segments on a straight line). On the other hand,
 * objects such as @p Triangulation<1,2> or @p Triangulation<2,3> (that are
 * associated with curves in 2D or surfaces in 3D) are the ones one wants to
 * use in the boundary element method.
 *
 * This class is written to be as independent of the dimension as possible
 * (thus the complex construction of the
 * dealii::internal::TriangulationImplementation::TriaLevel classes) to allow
 * code-sharing, to allow reducing the need to mirror changes in the code for
 * one dimension to the code for other dimensions. Nonetheless, some of the
 * functions are dependent of the dimension and there only exist specialized
 * versions for distinct dimensions.
 *
 * This class satisfies the
 * @ref ConceptMeshType "MeshType concept"
 * requirements.
 *
 * <h3>Structure and iterators</h3>
 *
 * The actual data structure of a Triangulation object is rather complex and
 * quite inconvenient if one attempted to operate on it directly, since data
 * is spread over quite a lot of arrays and other places. However, there are
 * ways powerful enough to work on these data structures without knowing their
 * exact relations. deal.II uses class local alias (see below) to make
 * things as easy and dimension independent as possible.
 *
 * The Triangulation class provides iterators which enable looping over all
 * cells without knowing the exact representation used to describe them. For
 * more information see the documentation of <tt>TriaIterator</tt>. Their
 * names are alias imported from the Iterators class (thus making them
 * local types to this class) and are as follows:
 *
 * <ul>
 * <li> <tt>cell_iterator</tt>: loop over all cells used in the Triangulation
 * <li> <tt>active_cell_iterator</tt>: loop over all active cells
 * </ul>
 *
 * For <tt>dim==1</tt>, these iterators are mapped as follows:
 *  @code
 *    using cell_iterator = line_iterator;
 *    using active_cell_iterator = active_line_iterator;
 *  @endcode
 * while for @p dim==2 we have the additional face iterator:
 *  @code
 *    using cell_iterator = quad_iterator;
 *    using active_cell_iterator = active_quad_iterator;
 *
 *    using face_iterator = line_iterator;
 *    using active_face_iterator = active_line_iterator;
 *  @endcode
 *
 * By using the cell iterators, you can write code independent of the spatial
 * dimension. The same applies for substructure iterators, where a
 * substructure is defined as a face of a cell. The face of a cell is a vertex
 * in 1D and a line in 2D; however, vertices are handled in a different way
 * and therefore lines have no faces.
 *
 * The Triangulation class offers functions like begin_active() which gives
 * you an iterator to the first active cell. There are quite a lot of
 * functions returning iterators. Take a look at the class doc to get an
 * overview.
 *
 * Usage of these iterators is similar to usage of standard container
 * iterators. Some examples taken from the Triangulation source code follow
 * (notice that in the last two examples the template parameter @p spacedim
 * has been omitted, so it takes the default value <code>dim</code>).
 *
 * <ul>
 * <li> <em>Counting the number of cells on a specific level</em>
 *    @code
 *      template <int dim, int spacedim>
 *      unsigned int
 *      Triangulation<dim, spacedim>::n_cells (const int level) const
 *      {
 *        int n=0;
 *        for (const auto &cell : cell_iterators_on_level(level))
 *          ++n;
 *        return n;
 *      }
 *    @endcode
 * Another way, which uses <tt>std::distance</tt>, would be to write
 *    @code
 *      template <int dim>
 *      unsigned int
 *      Triangulation<dim>::n_cells (const int level) const
 *      {
 *        int n=0;
 *        distance (begin(level),
 *                  (level == levels.size()-1 ?
 *                   cell_iterator(end()) :
 *                   begin (level+1)),
 *                  n);
 *        return n;
 *      }
 *    @endcode
 *
 * <li> <em>Refining all cells of a triangulation</em>
 *    @code
 *      template <int dim>
 *      void Triangulation<dim>::refine_global ()
 *      {
 *        for (const auto &cell : active_cell_iterators())
 *          cell->set_refine_flag ();
 *        execute_coarsening_and_refinement ();
 *      }
 *    @endcode
 * </ul>
 *
 *
 * <h3>Usage</h3>
 *
 * Usage of a Triangulation is mainly done through the use of iterators. An
 * example probably shows best how to use it:
 * @code
 * int main ()
 * {
 *   Triangulation<2> tria;
 *
 *   // read in a coarse grid file
 *
 *   // we want to log the refinement history
 *   ofstream history ("mesh.history");
 *
 *   // refine first cell
 *   tria.begin_active()->set_refine_flag();
 *   tria.save_refine_flags (history);
 *   tria.execute_coarsening_and_refinement ();
 *
 *   // refine first active cell on coarsest level
 *   tria.begin_active()->set_refine_flag ();
 *   tria.save_refine_flags (history);
 *   tria.execute_coarsening_and_refinement ();
 *
 *   Triangulation<2>::active_cell_iterator cell;
 *   for (int i=0; i<17; ++i)
 *     {
 *       // refine the presently second last cell 17 times
 *       cell = tria.last_active(tria.n_levels()-1);
 *       --cell;
 *       cell->set_refine_flag ();
 *       tria.save_refine_flags (history);
 *       tria.execute_coarsening_and_refinement ();
 *     };
 *   // output the grid
 *   ofstream out("grid.1");
 *   GridOut::write_gnuplot (tria, out);
 * }
 * @endcode
 *
 *
 * <h3>Creating a triangulation</h3>
 *
 * There are several possibilities to create a triangulation:
 * <ul>
 * <li> The most common domains, such as hypercubes (i.e. lines, squares,
 * cubes, etc), hyper-balls (circles, balls, ...) and some other, more weird
 * domains such as the L-shape region and higher dimensional generalizations
 * and others, are provided by the GridGenerator class which takes a
 * triangulation and fills it by a division of the required domain.
 *
 * <li> Reading in a triangulation: By using an object of the GridIn class,
 * you can read in fairly general triangulations. See there for more
 * information. The mentioned class uses the interface described directly
 * below to transfer the data into the triangulation.
 *
 * <li> Explicitly creating a triangulation: you can create a triangulation by
 * providing a list of vertices and a list of cells. Each such cell consists
 * of a vector storing the indices of the vertices of this cell in the vertex
 * list. To see how this works, you can take a look at the GridIn<dim>::read_*
 * functions. The appropriate function to be called is create_triangulation().
 *
 * Creating the hierarchical information needed for this library from cells
 * storing only vertex information can be quite a complex task.  For example
 * in 2D, we have to create lines between vertices (but only once, though
 * there are two cells which link these two vertices) and we have to create
 * neighborhood information. Grids being read in should therefore not be too
 * large, reading refined grids would be inefficient (although there is
 * technically no problem in reading grids with several 10.000 or 100.000
 * cells; the library can handle this without much problems). Apart from the
 * performance aspect, refined grids do not lend too well to multigrid
 * algorithms, since solving on the coarsest level is expensive. It is wiser
 * in any case to read in a grid as coarse as possible and then do the needed
 * refinement steps.
 *
 * It is your duty to guarantee that cells have the correct orientation. To
 * guarantee this, in the input vector keeping the cell list, the vertex
 * indices for each cell have to be in a defined order, see the documentation
 * of GeometryInfo<dim>. In one dimension, the first vertex index must refer
 * to that vertex with the lower coordinate value. In 2D and 3D, the
 * corresponding conditions are not easy to verify and no full attempt to do
 * so is made. If you violate this condition, you may end up with matrix
 * entries having the wrong sign (clockwise vertex numbering, which results in
 * a negative area element) of with wrong matrix elements (twisted
 * quadrilaterals, i.e. two vertices interchanged; this results in a wrong
 * area element).
 *
 * There are more subtle conditions which must be imposed upon the vertex
 * numbering within cells. They do not only hold for the data read from an UCD
 * or any other input file, but also for the data passed to
 * create_triangulation(). See the documentation for the GridIn class for more
 * details on this, and above all to the GridReordering class that explains
 * many of the problems and an algorithm to reorder cells such that they
 * satisfy the conditions outlined above.
 *
 * <li> Copying a triangulation: when computing on time dependent meshes or
 * when using adaptive refinement, you will often want to create a new
 * triangulation to be the same as another one. This is facilitated by the @p
 * copy_triangulation function.
 *
 * It is guaranteed that vertex, line or cell numbers in the two
 * triangulations are the same and that two iterators walking on the two
 * triangulations visit matching cells if they are incremented in parallel. It
 * may be conceivable to implement a clean-up in the copy operation, which
 * eliminates holes of unused memory, re-joins scattered data and so on. In
 * principle this would be a useful operation but guaranteeing some
 * parallelism in the two triangulations seems more important since usually
 * data will have to be transferred between the grids.
 * </ul>
 *
 * Finally, there is a special function for folks who like bad grids:
 * distort_random(). It moves all the vertices in the grid a bit around by a
 * random value, leaving behind a distorted mesh. Note that you should apply
 * this function to the final mesh, since refinement smoothes the mesh a bit.
 *
 * The function will make sure that vertices on restricted faces (hanging
 * nodes) will end up in the correct place, i.e. in the middle of the two
 * other vertices of the mother line, and the analogue in higher space
 * dimensions (vertices on the boundary are not corrected, so don't distort
 * boundary vertices in more than two space dimension, i.e. in dimensions
 * where boundary vertices can be hanging nodes). Applying the algorithm has
 * another drawback related to the placement of cells, however: the children
 * of a cell will not occupy the same region of the domain as the mother cell
 * does. While this is the usual behavior with cells at the boundary, here you
 * may get into trouble when using multigrid algorithms or when transferring
 * solutions from coarse to fine grids and back. In general, the use of this
 * function is only safe if you only use the most refined level of the
 * triangulation for computations.
 *
 *
 *
 * <h3>Refinement and coarsening of a triangulation</h3>
 *
 * Refinement of a triangulation may be done through several ways. The most
 * low-level way is directly through iterators: let @p i be an iterator to an
 * active cell (i.e. the cell pointed to has no children), then the function
 * call <tt>i->set_refine_flag()</tt> marks the respective cell for
 * refinement. Marking non-active cells results in an error.
 *
 * After all the cells you wanted to mark for refinement, call
 * execute_coarsening_and_refinement() to actually perform the refinement.
 * This function itself first calls the @p prepare_coarsening_and_refinement
 * function to regularize the resulting triangulation: since a face between
 * two adjacent cells may only be subdivided once (i.e. the levels of two
 * adjacent cells may differ by one at most; it is not possible to have a cell
 * refined twice while the neighboring one is not refined), some additional
 * cells are flagged for refinement to smooth the grid. This enlarges the
 * number of resulting cells but makes the grid more regular, thus leading to
 * better approximation properties and, above all, making the handling of data
 * structures and algorithms much easier. To be honest, this is mostly an
 * algorithmic step than one needed by the finite element method.
 *
 * To coarsen a grid, the same way as above is possible by using
 * <tt>i->set_coarsen_flag</tt> and calling
 * execute_coarsening_and_refinement().
 *
 * The reason for first coarsening, then refining is that the refinement
 * usually adds some additional cells to keep the triangulation regular and
 * thus satisfies all refinement requests, while the coarsening does not
 * delete cells not requested for; therefore the refinement will often revert
 * some effects of coarsening while the opposite is not true. The stated order
 * of coarsening before refinement will thus normally lead to a result closer
 * to the intended one.
 *
 * Marking cells for refinement 'by hand' through iterators is one way to
 * produce a new grid, especially if you know what kind of grid you are
 * looking for, e.g. if you want to have a grid successively refined towards
 * the boundary or always at the center (see the example programs, they do
 * exactly these things). There are more advanced functions, however, which
 * are more suitable for automatic generation of hierarchical grids in the
 * context of a posteriori error estimation and adaptive finite elements.
 * These functions can be found in the GridRefinement class.
 *
 *
 * <h3>Smoothing of a triangulation</h3>
 *
 * Some degradation of approximation properties has been observed for grids
 * which are too unstructured. Therefore, prepare_coarsening_and_refinement()
 * which is automatically called by execute_coarsening_and_refinement() can do
 * some smoothing of the triangulation. Note that mesh smoothing is only done
 * for two or more space dimensions, no smoothing is available at present for
 * one spatial dimension. In the following, let <tt>execute_*</tt> stand for
 * execute_coarsening_and_refinement().
 *
 * For the purpose of smoothing, the Triangulation constructor takes an
 * argument specifying whether a smoothing step shall be performed on the grid
 * each time <tt>execute_*</tt> is called. The default is that such a step not
 * be done, since this results in additional cells being produced, which may
 * not be necessary in all cases. If switched on, calling <tt>execute_*</tt>
 * results in flagging additional cells for refinement to avoid vertices as
 * the ones mentioned. The algorithms for both regularization and smoothing of
 * triangulations are described below in the section on technical issues. The
 * reason why this parameter must be given to the constructor rather than to
 * <tt>execute_*</tt> is that it would result in algorithmic problems if you
 * called <tt>execute_*</tt> once without and once with smoothing, since then
 * in some refinement steps would need to be refined twice.
 *
 * The parameter taken by the constructor is an integer which may be composed
 * bitwise by the constants defined in the enum #MeshSmoothing (see there for
 * the possibilities).
 *
 * @note While it is possible to pass all of the flags in #MeshSmoothing to
 * objects of type parallel::distributed::Triangulation, it is not always
 * possible to honor all of these smoothing options if they would require
 * knowledge of refinement/coarsening flags on cells not locally owned by this
 * processor. As a consequence, for some of these flags, the ultimate number
 * of cells of the parallel triangulation may depend on the number of
 * processors into which it is partitioned.
 *
 *
 * <h3>Material and boundary information</h3>
 *
 * Each cell, face or edge stores information denoting the material or the
 * part of the boundary that an object belongs to. The material id of a cell
 * is typically used to identify which cells belong to a particular part of
 * the domain, e.g., when you have different materials (steel, concrete, wood)
 * that are all part of the same domain. One would then usually query the
 * material id associated with a cell during assembly of the bilinear form,
 * and use it to determine (e.g., by table lookup, or a sequence of if-else
 * statements) what the correct material coefficients would be for that cell.
 * See also
 * @ref GlossMaterialId "this glossary entry".
 *
 * This material_id may be set upon construction of a triangulation (through
 * the CellData data structure), or later through use of cell iterators. For a
 * typical use of this functionality, see the step-28 tutorial program. The
 * functions of the GridGenerator namespace typically set the material ID of
 * all cells to zero. When reading a triangulation through the GridIn class,
 * different input file formats have different conventions, but typically
 * either explicitly specify the material id, or if they don't, then GridIn
 * simply sets them to zero. Because the material of a cell is intended
 * to pertain to a particular region of the domain, material ids are inherited
 * by child cells from their parent upon mesh refinement.
 *
 * Boundary indicators on lower dimensional objects (these have no material
 * id) indicate the number of a boundary component. The weak formulation of the
 * partial differential equation may have different boundary conditions on
 * different parts of the boundary. The boundary indicator can be used in
 * creating the matrix or the right hand side vector to indicate these
 * different parts of the model (this use is like the material id of cells).
 * Boundary indicators may be in the range from zero to
 * numbers::internal_face_boundary_id-1. The value
 * numbers::internal_face_boundary_id is reserved to denote interior lines (in
 * 2D) and interior lines and quads (in 3D), which do not have a boundary
 * indicator. This way, a program can easily determine, whether such an object
 * is at the boundary or not. Material indicators may be in the range from
 * zero to numbers::invalid_material_id-1.
 *
 * Lines in two dimensions and quads in three dimensions inherit their
 * boundary indicator to their children upon refinement. You should therefore
 * make sure that if you have different boundary parts, the different parts
 * are separated by a vertex (in 2D) or a line (in 3D) such that each boundary
 * line or quad has a unique boundary indicator.
 *
 * By default (unless otherwise specified during creation of a triangulation),
 * all parts of the boundary have boundary indicator zero. As a historical
 * wart, this isn't true for 1d meshes, however: For these, leftmost vertices
 * have boundary indicator zero while rightmost vertices have boundary
 * indicator one. In either case, the boundary indicator of a face can be
 * changed using a call of the kind
 * <code>cell-@>face(1)-@>set_boundary_id(42);</code>.
 *
 * @see
 * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
 *
 *
 * <h3>History of a triangulation</h3>
 *
 * It is possible to reconstruct a grid from its refinement history, which can
 * be stored and loaded through the @p save_refine_flags and @p
 * load_refine_flags functions. Normally, the code will look like this:
 *   @code
 *     // open output file
 *     std::ofstream history("mesh.history");
 *     // do 10 refinement steps
 *     for (unsigned int step=0; step<10; ++step)
 *       {
 *         ...;
 *         // flag cells according to some criterion
 *         ...;
 *         tria.save_refine_flags (history);
 *         tria.execute_coarsening_and_refinement ();
 *       }
 *   @endcode
 *
 * If you want to re-create the grid from the stored information, you write:
 *   @code
 *     // open input file
 *     std::ifstream history("mesh.history");
 *     // do 10 refinement steps
 *     for (unsigned int step=0; step<10; ++step)
 *       {
 *         tria.load_refine_flags (history);
 *         tria.execute_coarsening_and_refinement ();
 *       }
 *   @endcode
 *
 * The same scheme is employed for coarsening and the coarsening flags.
 *
 * You may write other information to the output file between different sets
 * of refinement information, as long as you read it upon re-creation of the
 * grid. You should make sure that the other information in the new
 * triangulation which is to be created from the saved flags, matches that of
 * the old triangulation, for example the smoothing level; if not, the cells
 * actually created from the flags may be other ones, since smoothing adds
 * additional cells, but their number may be depending on the smoothing level.
 *
 * There actually are two sets of <tt>save_*_flags</tt> and
 * <tt>load_*_flags</tt> functions. One takes a stream as argument and
 * reads/writes the information from/to the stream, thus enabling storing
 * flags to files. The other set takes an argument of type
 * <tt>vector<bool></tt>. This enables the user to temporarily store some
 * flags, e.g. if another function needs them, and restore them afterwards.
 *
 *
 * <h3>User flags and data</h3>
 *
 * A triangulation offers one bit per line, quad, etc for user flags. This
 * field can be accessed as all other data using iterators. Normally, this
 * user flag is used if an algorithm walks over all cells and needs
 * information whether another cell, e.g. a neighbor, has already been
 * processed. See
 * @ref GlossUserFlags "the glossary for more information".
 *
 * There is another set of user data, which can be either an <tt>unsigned
 * int</tt> or a <tt>void *</tt>, for each line, quad, etc. You can access
 * these through the functions listed under <tt>User data</tt> in the accessor
 * classes. Again, see
 * @ref GlossUserData "the glossary for more information".
 *
 * The value of these user indices or pointers is @p nullptr by default. Note
 * that the pointers are not inherited to children upon refinement. Still,
 * after a remeshing they are available on all cells, where they were set on
 * the previous mesh.
 *
 * The usual warning about the missing type safety of @p void pointers are
 * obviously in place here; responsibility for correctness of types etc lies
 * entirely with the user of the pointer.
 *
 * @note User pointers and user indices are stored in the same place. In order
 * to avoid unwanted conversions, Triangulation checks which one of them is in
 * use and does not allow access to the other one, until clear_user_data() has
 * been called.
 *
 *
 * <h3>Describing curved geometries</h3>
 *
 * deal.II implements all geometries (curved and otherwise) with classes
 * inheriting from Manifold; see the documentation of Manifold, step-49, or
 * the
 * @ref manifold
 * module for examples and a complete description of the algorithms. By
 * default, all cells in a Triangulation have a flat geometry, meaning that
 * all lines in the Triangulation are assumed to be straight. If a cell has a
 * manifold_id that is not equal to numbers::flat_manifold_id then the
 * Triangulation uses the associated Manifold object for computations on that
 * cell (e.g., cell refinement). Here is a quick example, taken from the
 * implementation of GridGenerator::hyper_ball(), that sets up a polar grid:
 *
 * @code
 * int main ()
 * {
 *   Triangulation<2> triangulation;
 *   const std::vector<Point<2>> vertices = {{-1.0,-1.0},
 *                                           {+1.0,-1.0},
 *                                           {-0.5,-0.5},
 *                                           {+0.5,-0.5},
 *                                           {-0.5,+0.5},
 *                                           {+1.0,+1.0},
 *                                           {-1.0,+1.0},
 *                                           {+1.0,+1.0}};
 *   const std::vector<std::array<int,GeometryInfo<2>::vertices_per_cell>>
 *     cell_vertices = {{0, 1, 2, 3},
 *                      {0, 2, 6, 4},
 *                      {2, 3, 4, 5},
 *                      {1, 7, 3, 5},
 *                      {6, 4, 7, 5}};
 *
 *   std::vector<CellData<2>> cells(cell_vertices.size(), CellData<2>());
 *   for (unsigned int i=0; i<cell_vertices.size(); ++i)
 *     for (unsigned int j=0; j<GeometryInfo<2>::vertices_per_cell; ++j)
 *       cells[i].vertices[j] = cell_vertices[i][j];
 *
 *   triangulation.create_triangulation (vertices, cells, SubCellData());
 *   triangulation.set_all_manifold_ids_on_boundary(42);
 *
 *   // set_manifold stores a copy of its second argument,
 *   // so a temporary is ookay
 *   triangulation.set_manifold(42, PolarManifold<2>());
 *   for (unsigned int i = 0; i < 4; ++i)
 *     {
 *       // refine all boundary cells
 *       for (const auto &cell : triangulation.active_cell_iterators())
 *         if (cell->at_boundary())
 *           cell->set_refine_flag();
 *
 *       triangulation.execute_coarsening_and_refinement();
 *     }
 * }
 * @endcode
 *
 * This will set up a grid where the boundary lines will be refined by
 * performing calculations in polar coordinates. When the mesh is refined the
 * cells adjacent to the boundary will use this new line midpoint (as well as
 * the other three midpoints and original cell vertices) to calculate the cell
 * midpoint with a transfinite interpolation: this propagates the curved
 * boundary into the interior in a smooth way. It is possible to generate a
 * better grid (which interpolates across all cells between two different
 * Manifold descriptions, instead of just going one cell at a time) by using
 * TransfiniteInterpolationManifold; see the documentation of that class for
 * more information.
 *
 * You should take note of one caveat: if you have concave boundaries, you
 * must make sure that a new boundary vertex does not lie too much inside the
 * cell which is to be refined. The reason is that the center vertex is placed
 * at the point which is a weighted average of the vertices of the original
 * cell, new face midpoints, and (in 3D) new line midpoints. Therefore if your
 * new boundary vertex is too near the center of the old quadrilateral or
 * hexahedron, the distance to the midpoint vertex will become too small, thus
 * generating distorted cells. This issue is discussed extensively in
 * @ref GlossDistorted "distorted cells".
 *
 * <h3>Getting notice when a triangulation changes</h3>
 *
 * There are cases where one object would like to know whenever a
 * triangulation is being refined, copied, or modified in a number of other
 * ways. This could of course be achieved if, in your user code, you tell
 * every such object whenever you are about to refine the triangulation, but
 * this will get tedious and is error prone. The Triangulation class
 * implements a more elegant way to achieve this: signals.
 *
 * In essence, a signal is an object (a member of the Triangulation class)
 * that another object can connect to. A connection is in essence that the
 * connecting object passes a function object taking a certain number and kind
 * of arguments. Whenever the owner of the signal wants to indicate a certain
 * kind of event, it 'triggers' the signal, which in turn means that all
 * connections of the signal are triggered: in other word, the function
 * objects are executed and can take the action that is necessary.
 *
 * As a simple example, the following code will print something to the output
 * every time the triangulation has just been refined:
 *   @code
 *     void f()
 *     {
 *       std::cout << "Triangulation has been refined." << std::endl;
 *     }
 *
 *     void run ()
 *     {
 *       Triangulation<dim> triangulation;
 *       // fill it somehow
 *       triangulation.signals.post_refinement.connect (&f);
 *       triangulation.refine_global (2);
 *     }
 *   @endcode
 * This code will produce output twice, once for each refinement cycle.
 *
 * A more interesting application would be the following, akin to what the
 * FEValues class does. This class stores a pointer to a triangulation and
 * also an iterator to the cell last handled (so that it can compare the
 * current cell with the previous one and, for example, decide that there is
 * no need to re-compute the Jacobian matrix if the new cell is a simple
 * translation of the previous one). However, whenever the triangulation is
 * modified, the iterator to the previously handled cell needs to be
 * invalidated since it now no longer points to any useful cell (or, at the
 * very least, points to something that may not necessarily resemble the cells
 * previously handled). The code would look something like this (the real code
 * has some more error checking and has to handle the case that subsequent
 * cells might actually belong to different triangulation, but that is of no
 * concern to us here):
 * @code
 * template <int dim>
 * class FEValues
 * {
 *   Triangulation<dim>::active_cell_iterator current_cell, previous_cell;
 * public:
 *   void reinit (Triangulation<dim>::active_cell_iterator &cell);
 *   void invalidate_previous_cell ();
 * };
 *
 * template <int dim>
 * void
 * FEValues<dim>::reinit (Triangulation<dim>::active_cell_iterator &cell)
 * {
 *   if (previous_cell.status() != valid)
 *     {
 *       // previous_cell has not been set. set it now, and register with the
 *       // triangulation that we want to be informed about mesh refinement
 *       previous_cell = current_cell;
 *       previous_cell->get_triangulation().signals.post_refinement.connect(
 *         std::bind (&FEValues<dim>::invalidate_previous_cell,
 *                    std::ref (*this)));
 *     }
 *   else
 *    previous_cell = current_cell;
 *
 *   current_cell = cell;
 *   // ... do something with the cell...
 * }
 *
 * template <int dim>
 * void
 * FEValues<dim>::invalidate_previous_cell ()
 * {
 *   previous_cell = Triangulation<dim>::active_cell_iterator();
 * }
 * @endcode
 * Here, whenever the triangulation is refined, it triggers the post-
 * refinement signal which calls the function object attached to it. This
 * function object is the member function
 * <code>FEValues<dim>::invalidate_previous_cell</code> where we have bound
 * the single argument (the <code>this</code> pointer of a member function
 * that otherwise takes no arguments) to the <code>this</code> pointer of the
 * FEValues object. Note how here there is no need for the code that owns the
 * triangulation and the FEValues object to inform the latter if the former is
 * refined. (In practice, the function would want to connect to some of the
 * other signals that the triangulation offers as well, in particular to
 * creation and deletion signals.)
 *
 * The Triangulation class has a variety of signals that indicate different
 * actions by which the triangulation can modify itself and potentially
 * require follow-up action elsewhere. Please refer to Triangulation::Signals
 * for details.
 *
 * <h3>Serializing (loading or storing) triangulations</h3>
 *
 * Like many other classes in deal.II, the Triangulation class can stream its
 * contents to an archive using BOOST's serialization facilities. The data so
 * stored can later be retrieved again from the archive to restore the
 * contents of this object. This facility is frequently used to save the state
 * of a program to disk for possible later resurrection, often in the context
 * of checkpoint/restart strategies for long running computations or on
 * computers that aren't very reliable (e.g. on very large clusters where
 * individual nodes occasionally fail and then bring down an entire MPI job).
 *
 * For technical reasons, writing and restoring a Triangulation object is not
 * trivial. The primary reason is that unlike many other objects,
 * triangulations rely on many other objects to which they store pointers or
 * with which they interface; for example, triangulations store pointers to
 * objects describing boundaries and manifolds, and they have signals that
 * store pointers to other objects so they can be notified of changes in the
 * triangulation (see the section on signals in this introduction). Since these
 * objects are owned by the user space (for example the user can create a custom
 * manifold object), they may not be serializable. So in cases like this,
 * boost::serialize can store a reference to an object instead of the pointer,
 * but the reference will never be satisfied at write time because the object
 * pointed to is not serialized. Clearly, at load time, boost::serialize will
 * not know where to let the pointer point to because it never gets to re-create
 * the object originally pointed to.
 *
 * For these reasons, saving a triangulation to an archive does not store all
 * information, but only certain parts. More specifically, the information
 * that is stored is everything that defines the mesh such as vertex
 * locations, vertex indices, how vertices are connected to cells, boundary
 * indicators, subdomain ids, material ids, etc. On the other hand, the
 * following information is not stored:
 *   - signals
 *   - pointers to Manifold objects previously set using
 * Triangulation::set_manifold On the other hand, since these are objects that
 * are usually set in user code, they can typically easily be set again in that
 * part of your code in which you re-load triangulations.
 *
 * In a sense, this approach to serialization means that re-loading a
 * triangulation is more akin to calling the
 * Triangulation::create_triangulation function and filling it with some
 * additional content, as that function also does not touch the signals and
 * boundary objects that belong to this triangulation. In keeping with this
 * analogy, the Triangulation::load function also triggers the same kinds of
 * signal as Triangulation::create_triangulation.
 *
 *
 * <h3>Technical details</h3>
 *
 * <h4>%Algorithms for mesh regularization and smoothing upon refinement</h4>
 *
 * We chose an inductive point of view: since upon creation of the
 * triangulation all cells are on the same level, all regularity assumptions
 * regarding the maximum difference in level of cells sharing a common face,
 * edge or vertex hold. Since we use the regularization and smoothing in each
 * step of the mesh history, when coming to the point of refining it further
 * the assumptions also hold.
 *
 * The regularization and smoothing is done in the @p
 * prepare_coarsening_and_refinement function, which is called by @p
 * execute_coarsening_and_refinement at the very beginning.  It decides which
 * additional cells to flag for refinement by looking at the old grid and the
 * refinement flags for each cell.
 *
 * <ul>
 * <li> <em>Regularization:</em> The algorithm walks over all cells checking
 * whether the present cell is flagged for refinement and a neighbor of the
 * present cell is refined once less than the present one. If so, flag the
 * neighbor for refinement. Because of the induction above, there may be no
 * neighbor with level two less than the present one.
 *
 * The neighbor thus flagged for refinement may induce more cells which need
 * to be refined. However, such cells which need additional refinement always
 * are on one level lower than the present one, so we can get away with only
 * one sweep over all cells if we do the loop in the reverse way, starting
 * with those on the highest level. This way, we may flag additional cells on
 * lower levels, but if these induce more refinement needed, this is performed
 * later on when we visit them in out backward running loop.
 *
 * <li> <em>Smoothing:</em>
 * <ul>
 * <li> @p limit_level_difference_at_vertices: First a list is set up which
 * stores for each vertex the highest level one of the adjacent cells belongs
 * to. Now, since we did smoothing in the previous refinement steps also, each
 * cell may only have vertices with levels at most one greater than the level
 * of the present cell.
 *
 * However, if we store the level plus one for cells marked for refinement, we
 * may end up with cells which have vertices of level two greater than the
 * cells level. We need to refine this cell also, and need thus also update
 * the levels of its vertices. This itself may lead to cells needing
 * refinement, but these are on lower levels, as above, which is why we may do
 * all kinds of additional flagging in one loop only.
 *
 * <li> @p eliminate_unrefined_islands: For each cell we count the number of
 * neighbors which are refined or flagged for refinement. If this exceeds the
 * number of neighbors which are not refined and not flagged for refinement,
 * then the current cell is flagged for refinement. Since this may lead to
 * cells on the same level which also will need refinement, we will need
 * additional loops of regularization and smoothing over all cells until
 * nothing changes any more.
 *
 * <li> <tt>eliminate_refined_*_islands</tt>: This one does much the same as
 * the above one, but for coarsening. If a cell is flagged for refinement or
 * if all of its children are active and if the number of neighbors which are
 * either active and not flagged for refinement, or not active but all
 * children flagged for coarsening equals the total number of neighbors, then
 * this cell's children are flagged for coarsening or (if this cell was
 * flagged for refinement) the refine flag is cleared.
 *
 * For a description of the distinction between the two versions of the flag
 * see above in the section about mesh smoothing in the general part of this
 * classes description.
 *
 * The same applies as above: several loops may be necessary.
 * </ul>
 * </ul>
 *
 * Regularization and smoothing are a bit complementary in that we check
 * whether we need to set additional refinement flags when being on a cell
 * flagged for refinement (regularization) or on a cell not flagged for
 * refinement. This makes readable programming easier.
 *
 * All the described algorithms apply only for more than one space dimension,
 * since for one dimension no restrictions apply. It may be necessary to apply
 * some smoothing for multigrid algorithms, but this has to be decided upon
 * later.
 *
 *
 * <h3>Warning</h3>
 *
 * It seems impossible to preserve @p constness of a triangulation through
 * iterator usage. Thus, if you declare pointers to a @p const triangulation
 * object, you should be well aware that you might involuntarily alter the
 * data stored in the triangulation.
 *
 * @ingroup grid aniso
 * @author Wolfgang Bangerth, 1998; Ralf Hartmann, 2005
 */
template <int dim, int spacedim = dim>
class Triangulation : public Subscriptor
{
private:
  /**
   * An internal alias to make the definition of the iterator classes
   * simpler.
   */
  using IteratorSelector =
    dealii::internal::TriangulationImplementation::Iterators<dim, spacedim>;

public:
  /**
   * Declare some symbolic names for mesh smoothing algorithms. The meaning of
   * these flags is documented in the Triangulation class.
   */
  enum MeshSmoothing
  {
    /**
     * No mesh smoothing at all, except that meshes have to remain one-
     * irregular.
     */
    none = 0x0,
    /**
     * It can be shown, that degradation of approximation occurs if the
     * triangulation contains vertices which are member of cells with levels
     * differing by more than one. One such example is the following:
     *
     * @image html limit_level_difference_at_vertices.png ""
     *
     * It would seem that in two space dimensions, the maximum jump in levels
     * between cells sharing a common vertex is two (as in the example above).
     * However, this is not true if more than four cells meet at a vertex. It
     * is not uncommon that a
     * @ref GlossCoarseMesh "coarse (initial) mesh" contains vertices at which
     * six or even eight cells meet, when small features of the domain have to
     * be resolved even on the coarsest mesh. In that case, the maximum
     * difference in levels is three or four, respectively. The problem gets
     * even worse in three space dimensions.
     *
     * Looking at an interpolation of the second derivative of the finite
     * element solution (assuming bilinear finite elements), one sees that the
     * numerical solution is almost totally wrong, compared with the true
     * second derivative. Indeed, on regular meshes, there exist sharp
     * estimations that the H<sup>2</sup>-error is only of order one, so we
     * should not be surprised; however, the numerical solution may show a
     * value for the second derivative which may be a factor of ten away from
     * the true value. These problems are located on the small cell adjacent
     * to the center vertex, where cells of non-subsequent levels meet, as
     * well as on the upper and right neighbor of this cell (but with a less
     * degree of deviation from the true value).
     *
     * If the smoothing indicator given to the constructor contains the bit
     * for #limit_level_difference_at_vertices, situations as the above one
     * are eliminated by also marking the upper right cell for refinement.
     *
     * In case of anisotropic refinement, the level of a cell is not linked to
     * the refinement of a cell as directly as in case of isotropic
     * refinement. Furthermore, a cell can be strongly refined in one
     * direction and not or at least much less refined in another. Therefore,
     * it is very difficult to decide, which cases should be excluded from the
     * refinement process. As a consequence, when using anisotropic
     * refinement, the #limit_level_difference_at_vertices flag must not be
     * set. On the other hand, the implementation of multigrid methods in
     * deal.II requires that this bit be set.
     */
    limit_level_difference_at_vertices = 0x1,
    /**
     * Single cells which are not refined and are surrounded by cells which
     * are refined usually also lead to a sharp decline in approximation
     * properties locally. The reason is that the nodes on the faces between
     * unrefined and refined cells are not real degrees of freedom but carry
     * constraints. The patch without additional degrees of freedom is thus
     * significantly larger then the unrefined cell itself. If in the
     * parameter passed to the constructor the bit for
     * #eliminate_unrefined_islands is set, all cells which are not flagged
     * for refinement but which are surrounded by more refined cells than
     * unrefined cells are flagged for refinement. Cells which are not yet
     * refined but flagged for that are accounted for the number of refined
     * neighbors. Cells on the boundary are not accounted for at all. An
     * unrefined island is, by this definition also a cell which (in 2D) is
     * surrounded by three refined cells and one unrefined one, or one
     * surrounded by two refined cells, one unrefined one and is at the
     * boundary on one side. It is thus not a true island, as the name of the
     * flag may indicate. However, no better name came to mind to the author
     * by now.
     */
    eliminate_unrefined_islands = 0x2,
    /**
     * A triangulation of patch level 1 consists of patches, i.e. of cells
     * that are refined once. This flag ensures that a mesh of patch level 1
     * is still of patch level 1 after coarsening and refinement. It is,
     * however, the user's responsibility to ensure that the mesh is of patch
     * level 1 before calling
     * Triangulation::execute_coarsening_and_refinement() the first time. The
     * easiest way to achieve this is by calling global_refine(1) straight
     * after creation of the triangulation.  It follows that if at least one
     * of the children of a cell is or will be refined than all children need
     * to be refined. If the #patch_level_1 flag is set, than the flags
     * #eliminate_unrefined_islands, #eliminate_refined_inner_islands and
     * #eliminate_refined_boundary_islands will be ignored as they will be
     * fulfilled automatically.
     */
    patch_level_1 = 0x4,
    /**
     * Each
     * @ref GlossCoarseMesh "coarse grid"
     * cell is refined at least once,
     * i.e., the triangulation
     * might have active cells on level 1 but not on level 0. This flag
     * ensures that a mesh which has coarsest_level_1 has still
     * coarsest_level_1 after coarsening and refinement. It is, however, the
     * user's responsibility to ensure that the mesh has coarsest_level_1
     * before calling execute_coarsening_and_refinement the first time. The
     * easiest way to achieve this is by calling global_refine(1) straight
     * after creation of the triangulation. It follows that active cells on
     * level 1 may not be coarsened.
     *
     * The main use of this flag is to ensure that each cell has at least one
     * neighbor in each coordinate direction (i.e. each cell has at least a
     * left or right, and at least an upper or lower neighbor in 2d). This is
     * a necessary precondition for some algorithms that compute finite
     * differences between cells. The DerivativeApproximation class is one of
     * these algorithms that require that a triangulation is coarsest_level_1
     * unless all cells already have at least one neighbor in each coordinate
     * direction on the coarsest level.
     */
    coarsest_level_1 = 0x8,
    /**
     * This flag is not included in @p maximum_smoothing. The flag is
     * concerned with the following case: consider the case that an unrefined
     * and a refined cell share a common face and that one of the children of
     * the refined cell along the common face is flagged for further
     * refinement. In that case, the resulting mesh would have more than one
     * hanging node along one or more of the edges of the triangulation, a
     * situation that is not allowed. Consequently, in order to perform the
     * refinement, the coarser of the two original cells is also going to be
     * refined.
     *
     * However, in many cases it is sufficient to refine the coarser of the
     * two original cells in an anisotropic way to avoid the case of multiple
     * hanging vertices on a single edge. Doing only the minimal anisotropic
     * refinement can save cells and degrees of freedom. By specifying this
     * flag, the library can produce these anisotropic refinements.
     *
     * The flag is not included by default since it may lead to
     * anisotropically refined meshes even though no cell has ever been
     * refined anisotropically explicitly by a user command. This surprising
     * fact may lead to programs that do the wrong thing since they are not
     * written for the additional cases that can happen with anisotropic
     * meshes, see the discussion in the introduction to step-30.
     */
    allow_anisotropic_smoothing = 0x10,
    /**
     * This algorithm seeks for isolated cells which are refined or flagged
     * for refinement. This definition is unlike that for
     * #eliminate_unrefined_islands, which would mean that an island is
     * defined as a cell which is refined but more of its neighbors are not
     * refined than are refined. For example, in 2D, a cell's refinement would
     * be reverted if at most one of its neighbors is also refined (or refined
     * but flagged for coarsening).
     *
     * The reason for the change in definition of an island is, that this
     * option would be a bit dangerous, since if you consider a chain of
     * refined cells (e.g. along a kink in the solution), the cells at the two
     * ends would be coarsened, after which the next outermost cells would
     * need to be coarsened. Therefore, only one loop of flagging cells like
     * this could be done to avoid eating up the whole chain of refined cells
     * (`chain reaction'...).
     *
     * This algorithm also takes into account cells which are not actually
     * refined but are flagged for refinement. If necessary, it takes away the
     * refinement flag.
     *
     * Actually there are two versions of this flag,
     * #eliminate_refined_inner_islands and
     * #eliminate_refined_boundary_islands. There first eliminates islands
     * defined by the definition above which are in the interior of the
     * domain, while the second eliminates only those islands if the cell is
     * at the boundary. The reason for this split of flags is that one often
     * wants to eliminate such islands in the interior while those at the
     * boundary may well be wanted, for example if one refines the mesh
     * according to a criterion associated with a boundary integral or if one
     * has rough boundary data.
     */
    eliminate_refined_inner_islands = 0x100,
    /**
     * The result of this flag is very similar to
     * #eliminate_refined_inner_islands. See the documentation there.
     */
    eliminate_refined_boundary_islands = 0x200,
    /**
     * This flag prevents the occurrence of unrefined islands. In more detail:
     * It prohibits the coarsening of a cell if 'most of the neighbors' will
     * be refined after the step.
     */
    do_not_produce_unrefined_islands = 0x400,

    /**
     * This flag sums up all smoothing algorithms which may be performed upon
     * refinement by flagging some more cells for refinement.
     */
    smoothing_on_refinement =
      (limit_level_difference_at_vertices | eliminate_unrefined_islands),
    /**
     * This flag sums up all smoothing algorithms which may be performed upon
     * coarsening by flagging some more cells for coarsening.
     */
    smoothing_on_coarsening =
      (eliminate_refined_inner_islands | eliminate_refined_boundary_islands |
       do_not_produce_unrefined_islands),

    /**
     * This flag includes all the above ones (therefore combines all
     * smoothing algorithms implemented), with the exception of anisotropic
     * smoothing.
     */
    maximum_smoothing = 0xffff ^ allow_anisotropic_smoothing
  };

  /**
   * An alias that is used to identify cell iterators. The concept of
   * iterators is discussed at length in the
   * @ref Iterators "iterators documentation module".
   *
   * The current alias identifies cells in a triangulation. The TriaIterator
   * class works like a pointer that when you dereference it yields an object
   * of type CellAccessor. CellAccessor is a class that identifies properties
   * that are specific to cells in a triangulation, but it is derived (and
   * consequently inherits) from TriaAccessor that describes what you can ask
   * of more general objects (lines, faces, as well as cells) in a
   * triangulation.
   *
   * @ingroup Iterators
   */
  using cell_iterator = TriaIterator<CellAccessor<dim, spacedim>>;

  /**
   * An alias that is used to identify
   * @ref GlossActive "active cell iterators".
   * The concept of iterators is discussed at length in the
   * @ref Iterators "iterators documentation module".
   *
   * The current alias identifies active cells in a triangulation. The
   * TriaActiveIterator class works like a pointer to active objects that when
   * you dereference it yields an object of type CellAccessor. CellAccessor is
   * a class that identifies properties that are specific to cells in a
   * triangulation, but it is derived (and consequently inherits) from
   * TriaAccessor that describes what you can ask of more general objects
   * (lines, faces, as well as cells) in a triangulation.
   *
   * @ingroup Iterators
   */
  using active_cell_iterator = TriaActiveIterator<CellAccessor<dim, spacedim>>;

  /**
   * An alias that is used to identify iterators that point to faces.
   * The concept of iterators is discussed at length in the
   * @ref Iterators "iterators documentation module".
   *
   * The current alias identifies faces in a triangulation. The
   * TriaIterator class works like a pointer to objects that when
   * you dereference it yields an object of type TriaAccessor, i.e.,
   * class that can be used to query geometric properties of faces
   * such as their vertices, their area, etc.
   *
   * @ingroup Iterators
   */
  using face_iterator = TriaIterator<TriaAccessor<dim - 1, dim, spacedim>>;

  /**
   * An alias that is used to identify iterators that point to active faces,
   * i.e., to faces that have no children. Active faces must be faces of at
   * least one active cell.
   *
   * Other than the "active" qualification, this alias is identical to the
   * @p face_iterator alias. In particular, dereferencing either yields
   * the same kind of object.
   *
   * @ingroup Iterators
   */
  using active_face_iterator =
    TriaActiveIterator<TriaAccessor<dim - 1, dim, spacedim>>;

  /**
   * An alias that defines an iterator type to iterate over
   * vertices of a mesh.  The concept of iterators is discussed at
   * length in the
   * @ref Iterators "iterators documentation module".
   *
   * @ingroup Iterators
   */
  using vertex_iterator = TriaIterator<dealii::TriaAccessor<0, dim, spacedim>>;

  /**
   * An alias that defines an iterator type to iterate over
   * vertices of a mesh.  The concept of iterators is discussed at
   * length in the
   * @ref Iterators "iterators documentation module".
   *
   * This alias is in fact identical to the @p vertex_iterator alias
   * above since all vertices in a mesh are active (i.e., are a vertex of
   * an active cell).
   *
   * @ingroup Iterators
   */
  using active_vertex_iterator =
    TriaActiveIterator<dealii::TriaAccessor<0, dim, spacedim>>;

  /**
   * An alias that defines an iterator over the (one-dimensional) lines
   * of a mesh. In one-dimensional meshes, these are the cells of the mesh,
   * whereas in two-dimensional meshes the lines are the faces of cells.
   *
   * @ingroup Iterators
   */
  using line_iterator = typename IteratorSelector::line_iterator;

  /**
   * An alias that allows iterating over the <i>active</i> lines, i.e.,
   * that subset of lines that have no children. In one-dimensional meshes,
   * these are the cells of the mesh, whereas in two-dimensional
   * meshes the lines are the faces of cells.
   *
   * In two- or three-dimensional meshes, lines without children (i.e.,
   * the active lines) are part of at least one active cell. Each such line may
   * additionally be a child of a line of a coarser cell adjacent to a cell
   * that is active. (This coarser neighbor would then also be active.)
   *
   * @ingroup Iterators
   */
  using active_line_iterator = typename IteratorSelector::active_line_iterator;

  /**
   * An alias that defines an iterator over the (two-dimensional) quads
   * of a mesh. In two-dimensional meshes, these are the cells of the mesh,
   * whereas in three-dimensional meshes the quads are the faces of cells.
   *
   * @ingroup Iterators
   */
  using quad_iterator = typename IteratorSelector::quad_iterator;

  /**
   * An alias that allows iterating over the <i>active</i> quads, i.e.,
   * that subset of quads that have no children. In two-dimensional meshes,
   * these are the cells of the mesh, whereas in three-dimensional
   * meshes the quads are the faces of cells.
   *
   * In three-dimensional meshes, quads without children (i.e.,
   * the active quads) are faces of at least one active cell. Each such quad may
   * additionally be a child of a quad face of a coarser cell adjacent to a cell
   * that is active. (This coarser neighbor would then also be active.)
   *
   * @ingroup Iterators
   */
  using active_quad_iterator = typename IteratorSelector::active_quad_iterator;

  /**
   * An alias that defines an iterator over the (three-dimensional) hexes
   * of a mesh. This iterator only makes sense in three-dimensional meshes,
   * where hexes are the cells of the mesh.
   *
   * @ingroup Iterators
   */
  using hex_iterator = typename IteratorSelector::hex_iterator;

  /**
   * An alias that allows iterating over the <i>active</i> hexes of a mesh.
   * This iterator only makes sense in three-dimensional meshes,
   * where hexes are the cells of the mesh. Consequently, in these
   * three-dimensional meshes, this iterator is equivalent to the
   * @p active_cell_iterator alias.
   *
   * @ingroup Iterators
   */
  using active_hex_iterator = typename IteratorSelector::active_hex_iterator;

  /**
   * A structure that is used as an exception object by the
   * create_triangulation() function to indicate which cells among the coarse
   * mesh cells are inverted or severely distorted (see the entry on
   * @ref GlossDistorted "distorted cells"
   * in the glossary).
   *
   * Objects of this kind are thrown by the create_triangulation() and
   * execute_coarsening_and_refinement() functions, and they can be caught in
   * user code if this condition is to be ignored. Note, however, that such
   * exceptions are only produced if the necessity for this check was
   * indicated when calling the constructor of the Triangulation class.
   *
   * A cell is called <i>deformed</i> if the determinant of the Jacobian of
   * the mapping from reference cell to real cell is negative at least at one
   * vertex. This computation is done using the
   * GeometryInfo::jacobian_determinants_at_vertices function.
   */
  struct DistortedCellList : public dealii::ExceptionBase
  {
    /**
     * Destructor. Empty, but needed for the sake of exception specification,
     * since the base class has this exception specification and the
     * automatically generated destructor would have a different one due to
     * member objects.
     */
    virtual ~DistortedCellList() noexcept override = default;

    /**
     * A list of those cells among the coarse mesh cells that are deformed or
     * whose children are deformed.
     */
    std::list<typename Triangulation<dim, spacedim>::cell_iterator>
      distorted_cells;
  };

  /**
   * Make the dimension available in function templates.
   */
  static const unsigned int dimension = dim;

  /**
   * Make the space-dimension available in function templates.
   */
  static const unsigned int space_dimension = spacedim;

  /**
   * Create an empty triangulation. Do not create any cells.
   *
   * @param smooth_grid Determines the level of smoothness of the mesh size
   * function that should be enforced upon mesh refinement.
   *
   * @param check_for_distorted_cells Determines whether the triangulation
   * should check whether any of the cells that are created by
   * create_triangulation() or execute_coarsening_and_refinement() are
   * distorted (see
   * @ref GlossDistorted "distorted cells").
   * If set, these two functions may throw an exception if they encounter
   * distorted cells.
   */
  Triangulation(const MeshSmoothing smooth_grid               = none,
                const bool          check_for_distorted_cells = false);

  /**
   * Copy constructor.
   *
   * You should really use the @p copy_triangulation function, so this
   * constructor is deleted. The reason for this is
   * that we may want to use triangulation objects in collections. However,
   * C++ containers require that the objects stored in them are copyable, so
   * we need to provide a copy constructor. On the other hand, copying
   * triangulations is so expensive that we do not want such objects copied by
   * accident, for example in compiler-generated temporary objects. By
   * defining a copy constructor but throwing an error, we satisfy the formal
   * requirements of containers, but at the same time disallow actual copies.
   * Finally, through the exception, one easily finds the places where code
   * has to be changed to avoid copies.
   */
  Triangulation(const Triangulation<dim, spacedim> &) = delete;

  /**
   * Move constructor.
   *
   * Create a new triangulation by stealing the internal data of another
   * triangulation.
   */
  Triangulation(Triangulation<dim, spacedim> &&tria) noexcept;

  /**
   * Move assignment operator.
   */
  Triangulation &
  operator=(Triangulation<dim, spacedim> &&tria) noexcept;

  /**
   * Delete the object and all levels of the hierarchy.
   */
  virtual ~Triangulation() override;

  /**
   * Reset this triangulation into a virgin state by deleting all data.
   *
   * Note that this operation is only allowed if no subscriptions to this
   * object exist any more, such as DoFHandler objects using it.
   */
  virtual void
  clear();

  /**
   * Set the mesh smoothing to @p mesh_smoothing. This overrides the
   * MeshSmoothing given to the constructor. It is allowed to call this
   * function only if the triangulation is empty.
   */
  virtual void
  set_mesh_smoothing(const MeshSmoothing mesh_smoothing);

  /**
   * Return the mesh smoothing requirements that are obeyed.
   */
  virtual const MeshSmoothing &
  get_mesh_smoothing() const;

  /**
   * Assign a manifold object to a certain part of the triangulation. If
   * an object with manifold number @p number is refined, this object is used
   * to find the location of new vertices (see the results section of step-49
   * for a more in-depth discussion of this, with examples).  It is also used
   * for non-linear (i.e.: non-Q1) transformations of cells to the unit cell
   * in shape function calculations.
   *
   * The @p manifold_object is not copied and MUST persist until the
   * triangulation is destroyed. This is also true for triangulations
   * generated from this one by @p copy_triangulation.
   *
   * It is possible to remove or replace the boundary object during the
   * lifetime of a non-empty triangulation. Usually, this is done before the
   * first refinement and is dangerous afterwards. Removal of a manifold
   * object is done by reset_manifold(). This operation then replaces the
   * manifold object given before by a straight manifold approximation.
   *
   * @ingroup manifold
   *
   * @see
   * @ref GlossManifoldIndicator "Glossary entry on manifold indicators"
   */
  void
  set_manifold(const types::manifold_id       number,
               const Manifold<dim, spacedim> &manifold_object);


  /**
   * Reset those parts of the triangulation with the given manifold_id
   * to use a FlatManifold object. This is the default state of a
   * non-curved triangulation, and undoes assignment of a different
   * Manifold object by the function of same name and two arguments.
   *
   * @ingroup manifold
   *
   * @deprecated This method has been deprecated. Use
   * Triangulation::reset_manifold() instead.
   *
   * @see
   * @ref GlossManifoldIndicator "Glossary entry on manifold indicators"
   */
  DEAL_II_DEPRECATED
  void
  set_manifold(const types::manifold_id number);

  /**
   * Reset those parts of the triangulation with the given
   * @p manifold_number to use a FlatManifold object. This is the
   * default state of a non-curved triangulation, and undoes
   * assignment of a different Manifold object by the function
   * Triangulation::set_manifold().
   *
   * @ingroup manifold
   *
   * @see
   * @ref GlossManifoldIndicator "Glossary entry on manifold indicators"
   */
  void
  reset_manifold(const types::manifold_id manifold_number);

  /**
   * Reset all parts of the triangulation, regardless of their
   * manifold_id, to use a FlatManifold object. This undoes assignment
   * of all Manifold objects by the function
   * Triangulation::set_manifold().
   *
   * @ingroup manifold
   *
   * @see
   * @ref GlossManifoldIndicator "Glossary entry on manifold indicators"
   */
  void
  reset_all_manifolds();

  /**
   * Set the manifold_id of all cells and faces to the given argument.
   *
   * @ingroup manifold
   *
   * @see
   * @ref GlossManifoldIndicator "Glossary entry on manifold indicators"
   */
  void
  set_all_manifold_ids(const types::manifold_id number);

  /**
   * Set the manifold_id of all boundary faces to the given argument.
   *
   * @ingroup manifold
   *
   * @see
   * @ref GlossManifoldIndicator "Glossary entry on manifold indicators"
   */
  void
  set_all_manifold_ids_on_boundary(const types::manifold_id number);

  /**
   * Set the manifold_id of all boundary faces and edges with given
   * boundary_id @p b_id to the given manifold_id @p number.
   *
   * @ingroup manifold
   *
   * @see
   * @ref GlossManifoldIndicator "Glossary entry on manifold indicators"
   */
  void
  set_all_manifold_ids_on_boundary(const types::boundary_id b_id,
                                   const types::manifold_id number);

  /**
   * Return a constant reference to a Manifold object used for this
   * triangulation.  Number is the same as in @p set_manifold
   *
   * @ingroup manifold
   *
   * @see
   * @ref GlossManifoldIndicator "Glossary entry on manifold indicators"
   */
  const Manifold<dim, spacedim> &
  get_manifold(const types::manifold_id number) const;

  /**
   * Return a vector containing all boundary indicators assigned to boundary
   * faces of this Triangulation object. Note, that each boundary indicator is
   * reported only once. The size of the return vector will represent the
   * number of different indicators (which is greater or equal one).
   *
   * @ingroup boundary
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  std::vector<types::boundary_id>
  get_boundary_ids() const;

  /**
   * Return a vector containing all manifold indicators assigned to the
   * objects of this Triangulation. Note, that each manifold indicator is
   * reported only once. The size of the return vector will represent the
   * number of different indicators (which is greater or equal one).
   *
   * @ingroup manifold
   *
   * @see
   * @ref GlossManifoldIndicator "Glossary entry on manifold indicators"
   */
  std::vector<types::manifold_id>
  get_manifold_ids() const;

  /**
   * Copy @p other_tria to this triangulation. This operation is not cheap, so
   * you should be careful with using this. We do not implement this function
   * as a copy constructor, since it makes it easier to maintain collections
   * of triangulations if you can assign them values later on.
   *
   * Keep in mind that this function also copies the pointer to the boundary
   * descriptor previously set by the @p set_manifold function. You must
   * therefore also guarantee that the Manifold objects describing the boundary
   * have a lifetime at least as long as the copied triangulation.
   *
   * This triangulation must be empty beforehand.
   *
   * The function is made @p virtual since some derived classes might want to
   * disable or extend the functionality of this function.
   *
   * @note Calling this function triggers the 'copy' signal on other_tria, i.e.
   * the triangulation being copied <i>from</i>.  It also triggers the
   * 'create' signal of the current triangulation. See the section on signals
   * in the general documentation for more information.
   *
   * @note The list of connections to signals is not copied from the old to
   * the new triangulation since these connections were established to monitor
   * how the old triangulation changes, not how any triangulation it may be
   * copied to changes.
   */
  virtual void
  copy_triangulation(const Triangulation<dim, spacedim> &other_tria);

  /**
   * Create a triangulation from a list of vertices and a list of cells, each
   * of the latter being a list of <tt>1<<dim</tt> vertex indices. The
   * triangulation must be empty upon calling this function and the cell list
   * should be useful (connected domain, etc.). The result of calling this
   * function is a
   * @ref GlossCoarseMesh "coarse mesh".
   *
   * Material data for the cells is given within the @p cells array, while
   * boundary information is given in the @p subcelldata field.
   *
   * The numbering of vertices within the @p cells array is subject to some
   * constraints; see the general class documentation for this.
   *
   * For conditions when this function can generate a valid triangulation, see
   * the documentation of this class, and the GridIn and GridReordering class.
   *
   * If the <code>check_for_distorted_cells</code> flag was specified upon
   * creation of this object, at the very end of its operation, the current
   * function walks over all cells and verifies that none of the cells is
   * deformed (see the entry on
   * @ref GlossDistorted "distorted cells"
   * in the glossary), where we call a cell deformed if the determinant of the
   * Jacobian of the mapping from reference cell to real cell is negative at
   * least at one of the vertices (this computation is done using the
   * GeometryInfo::jacobian_determinants_at_vertices function). If there are
   * deformed cells, this function throws an exception of kind
   * DistortedCellList. Since this happens after all data structures have been
   * set up, you can catch and ignore this exception if you know what you do
   * -- for example, it may be that the determinant is zero (indicating that
   * you have collapsed edges in a cell) but that this is ok because you
   * didn't intend to integrate on this cell anyway. On the other hand,
   * deformed cells are often a sign of a mesh that is too coarse to resolve
   * the geometry of the domain, and in this case ignoring the exception is
   * probably unwise.
   *
   * @note This function is used in step-14 .
   *
   * @note This function triggers the create signal after doing its work. See
   * the section on signals in the general documentation of this class.
   *
   * @note The check for distorted cells is only done if dim==spacedim, as
   * otherwise cells can legitimately be twisted if the manifold they describe
   * is twisted.
   */
  virtual void
  create_triangulation(const std::vector<Point<spacedim>> &vertices,
                       const std::vector<CellData<dim>> &  cells,
                       const SubCellData &                 subcelldata);

  /**
   * For backward compatibility, only. This function takes the cell data in
   * the ordering as requested by deal.II versions up to 5.2, converts it to
   * the new (lexicographic) ordering and calls create_triangulation().
   *
   * @note This function internally calls create_triangulation and therefore
   * can throw the same exception as the other function.
   */
  virtual void
  create_triangulation_compatibility(
    const std::vector<Point<spacedim>> &vertices,
    const std::vector<CellData<dim>> &  cells,
    const SubCellData &                 subcelldata);

  /**
   * Revert or flip the direction_flags of a dim<spacedim triangulation, see
   * @ref GlossDirectionFlag.
   *
   * This function throws an exception if dim equals spacedim.
   */
  void
  flip_all_direction_flags();

  /**
   * @name Mesh refinement
   * @{
   */

  /**
   * Flag all active cells for refinement.  This will refine all cells of all
   * levels which are not already refined (i.e. only cells are refined which
   * do not yet have children). The cells are only flagged, not refined, thus
   * you have the chance to save the refinement flags.
   */
  void
  set_all_refine_flags();

  /**
   * Refine all cells @p times times, by alternatingly calling
   * set_all_refine_flags and execute_coarsening_and_refinement.
   *
   * The latter function may throw an exception if it creates cells that are
   * distorted (see its documentation for an explanation). This exception will
   * be propagated through this function if that happens, and you may not get
   * the actual number of refinement steps in that case.
   *
   * @note This function triggers the pre- and post-refinement signals before
   * and after doing each individual refinement cycle (i.e. more than once if
   * times > 1) . See the section on signals in the general documentation of
   * this class.
   */
  void
  refine_global(const unsigned int times = 1);

  /**
   * Execute both refinement and coarsening of the triangulation.
   *
   * The function resets all refinement and coarsening flags to false. It uses
   * the user flags for internal purposes. They will therefore be overwritten
   * by undefined content.
   *
   * To allow user programs to fix up these cells if that is desired, this
   * function after completing all other work may throw an exception of type
   * DistortedCellList that contains a list of those cells that have been
   * refined and have at least one child that is distorted. The function does
   * not create such an exception if no cells have created distorted children.
   * Note that for the check for distorted cells to happen, the
   * <code>check_for_distorted_cells</code> flag has to be specified upon
   * creation of a triangulation object.
   *
   * See the general docs for more information.
   *
   * @note This function triggers the pre- and post-refinement signals before
   * and after doing its work. See the section on signals in the general
   * documentation of this class.
   *
   * @note If the boundary description is sufficiently irregular, it can
   * happen that some of the children produced by mesh refinement are
   * distorted (see the extensive discussion on
   * @ref GlossDistorted "distorted cells").
   *
   * @note This function is <tt>virtual</tt> to allow derived classes to
   * insert hooks, such as saving refinement flags and the like (see e.g. the
   * PersistentTriangulation class).
   */
  virtual void
  execute_coarsening_and_refinement();

  /**
   * Do both preparation for refinement and coarsening as well as mesh
   * smoothing.
   *
   * Regarding the refinement process it fixes the closure of the refinement
   * in <tt>dim>=2</tt> (make sure that no two cells are adjacent with a
   * refinement level differing with more than one), etc.  It performs some
   * mesh smoothing if the according flag was given to the constructor of this
   * class.  The function returns whether additional cells have been flagged
   * for refinement.
   *
   * See the general doc of this class for more information on smoothing upon
   * refinement.
   *
   * Regarding the coarsening part, flagging and deflagging cells in
   * preparation of the actual coarsening step are done. This includes
   * deleting coarsen flags from cells which may not be deleted (e.g. because
   * one neighbor is more refined than the cell), doing some smoothing, etc.
   *
   * The effect is that only those cells are flagged for coarsening which will
   * actually be coarsened. This includes the fact that all flagged cells
   * belong to parent cells of which all children are flagged.
   *
   * The function returns whether some cells' flagging has been changed in the
   * process.
   *
   * This function uses the user flags, so store them if you still need them
   * afterwards.
   */
  virtual bool
  prepare_coarsening_and_refinement();

  /*
   * @}
   */

  /**
   * @name Keeping up with what happens to a triangulation
   * @{
   */


  /**
   * Used to inform functions in derived classes how the cell with the given
   * cell_iterator is going to change. Note that this may me different than
   * the refine_flag() and coarsen_flag() in the cell_iterator in parallel
   * calculations because of refinement constraints that this machine does not
   * see.
   */
  enum CellStatus
  {
    /**
     * The cell will not be refined or coarsened and might or might not move
     * to a different processor.
     */
    CELL_PERSIST,
    /**
     * The cell will be or was refined.
     */
    CELL_REFINE,
    /**
     * The children of this cell will be or were coarsened into this cell.
     */
    CELL_COARSEN,
    /**
     * Invalid status. Will not occur for the user.
     */
    CELL_INVALID
  };

  /**
   * A structure used to accumulate the results of the cell_weights slot
   * functions below. It takes an iterator range and returns the sum of
   * values.
   */
  template <typename T>
  struct CellWeightSum
  {
    using result_type = T;

    template <typename InputIterator>
    T
    operator()(InputIterator first, InputIterator last) const
    {
      return std::accumulate(first, last, T());
    }
  };

  /**
   * A structure that has boost::signal objects for a number of actions that a
   * triangulation can do to itself. Please refer to the "Getting notice when
   * a triangulation changes" section in the general documentation of the
   * Triangulation class for more information and examples.
   *
   * For documentation on signals, see
   * http://www.boost.org/doc/libs/release/libs/signals2 .
   */
  struct Signals
  {
    /**
     * This signal is triggered whenever the
     * Triangulation::create_triangulation or
     * Triangulation::copy_triangulation() is called. This signal is also
     * triggered when loading a triangulation from an archive via
     * Triangulation::load().
     */
    boost::signals2::signal<void()> create;

    /**
     * This signal is triggered at the beginning of execution of the
     * Triangulation::execute_coarsening_and_refinement() function (which is
     * itself called by other functions such as Triangulation::refine_global()
     * ). At the time this signal is triggered, the triangulation is still
     * unchanged.
     */
    boost::signals2::signal<void()> pre_refinement;

    /**
     * This signal is triggered at the end of execution of the
     * Triangulation::execute_coarsening_and_refinement() function when the
     * triangulation has reached its final state.
     */
    boost::signals2::signal<void()> post_refinement;

    /**
     * This signal is triggered at the beginning of execution of the
     * GridTools::partition_triangulation() and
     * GridTools::partition_triangulation_zorder() functions. At the time this
     * signal is triggered, the triangulation is still unchanged.
     */
    boost::signals2::signal<void()> pre_partition;

    /**
     * This signal is triggered when a function in deal.II moves the grid
     * points of a mesh, e.g. GridTools::transform. Unfortunately,
     * modification of a vertex in user code through
     * <code>cell_iterator->vertex(v) = xxxx</code> cannot be detected by this
     * method.
     */
    boost::signals2::signal<void()> mesh_movement;

    /**
     * This signal is triggered for each cell that is going to be coarsened.
     *
     * @note This signal is triggered with the immediate parent cell of a set
     * of active cells as argument. The children of this parent cell will
     * subsequently be coarsened away.
     */
    boost::signals2::signal<void(
      const typename Triangulation<dim, spacedim>::cell_iterator &cell)>
      pre_coarsening_on_cell;

    /**
     * This signal is triggered for each cell that just has been refined.
     *
     * @note The signal parameter @p cell corresponds to the immediate parent
     * cell of a set of newly created active cells.
     */
    boost::signals2::signal<void(
      const typename Triangulation<dim, spacedim>::cell_iterator &cell)>
      post_refinement_on_cell;

    /**
     * This signal is triggered whenever the triangulation owning the signal
     * is copied by another triangulation using
     * Triangulation::copy_triangulation() (i.e. it is triggered on the
     * <i>old</i> triangulation, but the new one is passed as an argument).
     */
    boost::signals2::signal<void(
      const Triangulation<dim, spacedim> &destination_tria)>
      copy;

    /**
     * This signal is triggered whenever the Triangulation::clear() function
     * is called and in the destructor of the triangulation. This signal is
     * also triggered when loading a triangulation from an archive via
     * Triangulation::load() as the previous content of the triangulation is
     * first destroyed.
     *
     * The signal is triggered before the data structures of the
     * triangulation are destroyed. In other words, the functions
     * attached to this signal get a last look at the triangulation,
     * for example to save information stored as part of the
     * triangulation.
     */
    boost::signals2::signal<void()> clear;

    /**
     * This is a catch-all signal that is triggered whenever the create,
     * post_refinement, or clear signals are triggered. In effect, it can be
     * used to indicate to an object connected to the signal that the
     * triangulation has been changed, whatever the exact cause of the change.
     *
     * @note The cell-level signals @p pre_coarsening_on_cell and @p
     * post_refinement_on_cell are not connected to this signal.
     */
    boost::signals2::signal<void()> any_change;

    /**
     * This signal is triggered for each cell during every automatic or manual
     * repartitioning. This signal is somewhat special in that it is only
     * triggered for distributed parallel calculations and only if functions
     * are connected to it. It is intended to allow a weighted repartitioning
     * of the domain to balance the computational load across processes in a
     * different way than balancing the number of cells. Any connected
     * function is expected to take an iterator to a cell, and a CellStatus
     * argument that indicates whether this cell is going to be refined,
     * coarsened or left untouched (see the documentation of the CellStatus
     * enum for more information). The function is expected to return an
     * unsigned integer, which is interpreted as the additional computational
     * load of this cell. If this cell is going to be coarsened, the signal is
     * called for the parent cell and you need to provide the weight of the
     * future parent cell. If this cell is going to be refined the function
     * should return a weight, which will be equally assigned to every future
     * child cell of the current cell. As a reference a value of 1000 is added
     * for every cell to the total weight. This means a signal return value of
     * 1000 (resulting in a weight of 2000) means that it is twice as
     * expensive for a process to handle this particular cell. If several
     * functions are connected to this signal, their return values will be
     * summed to calculate the final weight.
     */
    boost::signals2::signal<unsigned int(const cell_iterator &,
                                         const CellStatus),
                            CellWeightSum<unsigned int>>
      cell_weight;

    /**
     * This signal is triggered at the beginning of execution of the
     * parallel::distributed::Triangulation::execute_coarsening_and_refinement()
     * function (which is
     * itself called by other functions such as Triangulation::refine_global()
     * ). At the time this signal is triggered, the triangulation is still
     * unchanged. This signal
     * is different from the pre_refinement signal, because in the parallel
     * distributed case the pre_refinement signal is triggered multiple times
     * without a way to distinguish the last signal call.
     */
    boost::signals2::signal<void()> pre_distributed_refinement;

    /**
     * This signal is triggered at the end of execution of the
     * parallel::distributed::Triangulation::execute_coarsening_and_refinement()
     * function when the triangulation has reached its final state. This signal
     * is different from the post_refinement signal, because in the parallel
     * distributed case the post_refinement signal is triggered multiple times
     * without a way to distinguish the last signal call.
     */
    boost::signals2::signal<void()> post_distributed_refinement;

    /**
     * This signal is triggered at the beginning of execution of the
     * parallel::distributed::Triangulation::repartition() function. At the time
     * this signal is triggered, the triangulation is still unchanged.
     *
     * @note The parallel::distributed::Triangulation::repartition() function is
     * also called by parallel::distributed::Triangulation::load(). Thus, the
     * pre_distributed_repartition signal will be triggered after the
     * pre_distributed_load one.
     */
    boost::signals2::signal<void()> pre_distributed_repartition;

    /**
     * This signal is triggered at the end of execution of the
     * parallel::distributed::Triangulation::repartition()
     * function when the triangulation has reached its final state.
     */
    boost::signals2::signal<void()> post_distributed_repartition;

    /**
     * This signal is triggered at the beginning of execution of the
     * parallel::distributed::Triangulation::save()
     * function. At the time this signal is triggered, the triangulation
     * is still unchanged.
     */
    boost::signals2::signal<void()> pre_distributed_save;

    /**
     * This signal is triggered at the end of execution of the
     * parallel::distributed::Triangulation::save()
     * function when the triangulation has reached its final state.
     */
    boost::signals2::signal<void()> post_distributed_save;

    /**
     * This signal is triggered at the beginning of execution of the
     * parallel::distributed::Triangulation::load()
     * function. At the time this signal is triggered, the triangulation
     * is still unchanged.
     */
    boost::signals2::signal<void()> pre_distributed_load;

    /**
     * This signal is triggered at the end of execution of the
     * parallel::distributed::Triangulation::load()
     * function when the triangulation has reached its final state.
     */
    boost::signals2::signal<void()> post_distributed_load;
  };

  /**
   * Signals for the various actions that a triangulation can do to itself.
   */
  mutable Signals signals;

  /*
   * @}
   */

  /**
   * @name History of a triangulation
   * @{
   */

  /**
   * Save the addresses of the cells which are flagged for refinement to @p
   * out.  For usage, read the general documentation for this class.
   */
  void
  save_refine_flags(std::ostream &out) const;

  /**
   * Same as above, but store the flags to a bitvector rather than to a file.
   */
  void
  save_refine_flags(std::vector<bool> &v) const;

  /**
   * Read the information stored by @p save_refine_flags.
   */
  void
  load_refine_flags(std::istream &in);

  /**
   * Read the information stored by @p save_refine_flags.
   */
  void
  load_refine_flags(const std::vector<bool> &v);

  /**
   * Analogue to @p save_refine_flags.
   */
  void
  save_coarsen_flags(std::ostream &out) const;

  /**
   * Same as above, but store the flags to a bitvector rather than to a file.
   */
  void
  save_coarsen_flags(std::vector<bool> &v) const;

  /**
   * Analogue to @p load_refine_flags.
   */
  void
  load_coarsen_flags(std::istream &out);

  /**
   * Analogue to @p load_refine_flags.
   */
  void
  load_coarsen_flags(const std::vector<bool> &v);

  /**
   * Return whether this triangulation has ever undergone anisotropic (as
   * opposed to only isotropic) refinement.
   */
  bool
  get_anisotropic_refinement_flag() const;

  /*
   * @}
   */

  /**
   * @name User data
   * @{
   */

  /**
   * Clear all user flags.  See also
   * @ref GlossUserFlags.
   */
  void
  clear_user_flags();

  /**
   * Save all user flags. See the general documentation for this class and the
   * documentation for the @p save_refine_flags for more details.  See also
   * @ref GlossUserFlags.
   */
  void
  save_user_flags(std::ostream &out) const;

  /**
   * Same as above, but store the flags to a bitvector rather than to a file.
   * The output vector is resized if necessary.  See also
   * @ref GlossUserFlags.
   */
  void
  save_user_flags(std::vector<bool> &v) const;

  /**
   * Read the information stored by @p save_user_flags.  See also
   * @ref GlossUserFlags.
   */
  void
  load_user_flags(std::istream &in);

  /**
   * Read the information stored by @p save_user_flags.  See also
   * @ref GlossUserFlags.
   */
  void
  load_user_flags(const std::vector<bool> &v);

  /**
   * Clear all user flags on lines.  See also
   * @ref GlossUserFlags.
   */
  void
  clear_user_flags_line();

  /**
   * Save the user flags on lines.  See also
   * @ref GlossUserFlags.
   */
  void
  save_user_flags_line(std::ostream &out) const;

  /**
   * Same as above, but store the flags to a bitvector rather than to a file.
   * The output vector is resized if necessary.  See also
   * @ref GlossUserFlags.
   */
  void
  save_user_flags_line(std::vector<bool> &v) const;

  /**
   * Load the user flags located on lines.  See also
   * @ref GlossUserFlags.
   */
  void
  load_user_flags_line(std::istream &in);

  /**
   * Load the user flags located on lines.  See also
   * @ref GlossUserFlags.
   */
  void
  load_user_flags_line(const std::vector<bool> &v);

  /**
   * Clear all user flags on quads.  See also
   * @ref GlossUserFlags.
   */
  void
  clear_user_flags_quad();

  /**
   * Save the user flags on quads.  See also
   * @ref GlossUserFlags.
   */
  void
  save_user_flags_quad(std::ostream &out) const;

  /**
   * Same as above, but store the flags to a bitvector rather than to a file.
   * The output vector is resized if necessary.  See also
   * @ref GlossUserFlags.
   */
  void
  save_user_flags_quad(std::vector<bool> &v) const;

  /**
   * Load the user flags located on quads.  See also
   * @ref GlossUserFlags.
   */
  void
  load_user_flags_quad(std::istream &in);

  /**
   * Load the user flags located on quads.  See also
   * @ref GlossUserFlags.
   */
  void
  load_user_flags_quad(const std::vector<bool> &v);


  /**
   * Clear all user flags on quads.  See also
   * @ref GlossUserFlags.
   */
  void
  clear_user_flags_hex();

  /**
   * Save the user flags on hexs.  See also
   * @ref GlossUserFlags.
   */
  void
  save_user_flags_hex(std::ostream &out) const;

  /**
   * Same as above, but store the flags to a bitvector rather than to a file.
   * The output vector is resized if necessary.  See also
   * @ref GlossUserFlags.
   */
  void
  save_user_flags_hex(std::vector<bool> &v) const;

  /**
   * Load the user flags located on hexs.  See also
   * @ref GlossUserFlags.
   */
  void
  load_user_flags_hex(std::istream &in);

  /**
   * Load the user flags located on hexs.  See also
   * @ref GlossUserFlags.
   */
  void
  load_user_flags_hex(const std::vector<bool> &v);

  /**
   * Clear all user pointers and indices and allow the use of both for next
   * access.  See also
   * @ref GlossUserData.
   */
  void
  clear_user_data();

  /**
   * Save all user indices. The output vector is resized if necessary. See
   * also
   * @ref GlossUserData.
   */
  void
  save_user_indices(std::vector<unsigned int> &v) const;

  /**
   * Read the information stored by save_user_indices().  See also
   * @ref GlossUserData.
   */
  void
  load_user_indices(const std::vector<unsigned int> &v);

  /**
   * Save all user pointers. The output vector is resized if necessary.  See
   * also
   * @ref GlossUserData.
   */
  void
  save_user_pointers(std::vector<void *> &v) const;

  /**
   * Read the information stored by save_user_pointers().  See also
   * @ref GlossUserData.
   */
  void
  load_user_pointers(const std::vector<void *> &v);

  /**
   * Save the user indices on lines. The output vector is resized if
   * necessary.  See also
   * @ref GlossUserData.
   */
  void
  save_user_indices_line(std::vector<unsigned int> &v) const;

  /**
   * Load the user indices located on lines.  See also
   * @ref GlossUserData.
   */
  void
  load_user_indices_line(const std::vector<unsigned int> &v);

  /**
   * Save the user indices on quads. The output vector is resized if
   * necessary.  See also
   * @ref GlossUserData.
   */
  void
  save_user_indices_quad(std::vector<unsigned int> &v) const;

  /**
   * Load the user indices located on quads.  See also
   * @ref GlossUserData.
   */
  void
  load_user_indices_quad(const std::vector<unsigned int> &v);

  /**
   * Save the user indices on hexes. The output vector is resized if
   * necessary.  See also
   * @ref GlossUserData.
   */
  void
  save_user_indices_hex(std::vector<unsigned int> &v) const;

  /**
   * Load the user indices located on hexs.  See also
   * @ref GlossUserData.
   */
  void
  load_user_indices_hex(const std::vector<unsigned int> &v);
  /**
   * Save the user indices on lines. The output vector is resized if
   * necessary.  See also
   * @ref GlossUserData.
   */
  void
  save_user_pointers_line(std::vector<void *> &v) const;

  /**
   * Load the user pointers located on lines.  See also
   * @ref GlossUserData.
   */
  void
  load_user_pointers_line(const std::vector<void *> &v);

  /**
   * Save the user pointers on quads. The output vector is resized if
   * necessary.  See also
   * @ref GlossUserData.
   */
  void
  save_user_pointers_quad(std::vector<void *> &v) const;

  /**
   * Load the user pointers located on quads.  See also
   * @ref GlossUserData.
   */
  void
  load_user_pointers_quad(const std::vector<void *> &v);

  /**
   * Save the user pointers on hexes. The output vector is resized if
   * necessary.  See also
   * @ref GlossUserData.
   */
  void
  save_user_pointers_hex(std::vector<void *> &v) const;

  /**
   * Load the user pointers located on hexs.  See also
   * @ref GlossUserData.
   */
  void
  load_user_pointers_hex(const std::vector<void *> &v);

  /*
   * @}
   */

  /**
   * @name Cell iterator functions
   * @{
   */

  /**
   * Iterator to the first used cell on level @p level.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  cell_iterator
  begin(const unsigned int level = 0) const;

  /**
   * Iterator to the first active cell on level @p level. If the given level
   * does not contain any active cells (i.e., all cells on this level are
   * further refined, then this function returns
   * <code>end_active(level)</code> so that loops of the kind
   * @code
   *   for (const auto cell=tria.begin_active(level);
   *        cell!=tria.end_active(level);
   *        ++cell)
   *     {
   *       ...
   *     }
   *  @endcode
   * have zero iterations, as may be expected if there are no active cells on
   * this level.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  active_cell_iterator
  begin_active(const unsigned int level = 0) const;

  /**
   * Iterator past the end; this iterator serves for comparisons of iterators
   * with past-the-end or before-the-beginning states.
   */
  cell_iterator
  end() const;

  /**
   * Return an iterator which is the first iterator not on level. If @p level
   * is the last level, then this returns <tt>end()</tt>.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  cell_iterator
  end(const unsigned int level) const;

  /**
   * Return an active iterator which is the first active iterator not on the
   * given level. If @p level is the last level, then this returns
   * <tt>end()</tt>.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  active_cell_iterator
  end_active(const unsigned int level) const;


  /**
   * Return an iterator pointing to the last used cell.
   */
  cell_iterator
  last() const;

  /**
   * Return an iterator pointing to the last active cell.
   */
  active_cell_iterator
  last_active() const;

  /**
   * @name Cell iterator functions returning ranges of iterators
   */

  /**
   * Return an iterator range that contains all cells (active or not) that
   * make up this triangulation. Such a range is useful to initialize range-
   * based for loops as supported by C++11. See the example in the
   * documentation of active_cell_iterators().
   *
   * @return The half open range <code>[this->begin(), this->end())</code>
   *
   * @ingroup CPP11
   */
  IteratorRange<cell_iterator>
  cell_iterators() const;

  /**
   * Return an iterator range that contains all active cells that make up this
   * triangulation. Such a range is useful to initialize range-based for loops
   * as supported by C++11, see also
   * @ref CPP11 "C++11 standard".
   *
   * Range-based for loops are useful in that they require much less code than
   * traditional loops (see <a href="http://en.wikipedia.org/wiki/C%2B%2B11
   * #Range-based_for_loop">here</a> for a discussion of how they work). An
   * example is that without range-based for loops, one often writes code such
   * as the following (assuming for a moment that our goal is setting the user
   * flag on every active cell):
   * @code
   *   Triangulation<dim> triangulation;
   *   ...
   *   typename Triangulation<dim>::active_cell_iterator
   *     cell = triangulation.begin_active(),
   *     endc = triangulation.end();
   *   for (; cell!=endc; ++cell)
   *     cell->set_user_flag();
   * @endcode
   * Using C++11's range-based for loops, this is now entirely equivalent to
   * the following:
   * @code
   *   Triangulation<dim> triangulation;
   *   ...
   *   for (const auto &cell : triangulation.active_cell_iterators())
   *     cell->set_user_flag();
   * @endcode
   *
   * @return The half open range <code>[this->begin_active(),
   * this->end())</code>
   *
   * @ingroup CPP11
   */
  IteratorRange<active_cell_iterator>
  active_cell_iterators() const;

  /**
   * Return an iterator range that contains all cells (active or not) that
   * make up the given level of this triangulation. Such a range is useful to
   * initialize range-based for loops as supported by C++11. See the example
   * in the documentation of active_cell_iterators().
   *
   * @param[in] level A given level in the refinement hierarchy of this
   * triangulation.
   * @return The half open range <code>[this->begin(level),
   * this->end(level))</code>
   *
   * @pre level must be less than this->n_levels().
   *
   * @ingroup CPP11
   */
  IteratorRange<cell_iterator>
  cell_iterators_on_level(const unsigned int level) const;

  /**
   * Return an iterator range that contains all active cells that make up the
   * given level of this triangulation. Such a range is useful to initialize
   * range-based for loops as supported by C++11. See the example in the
   * documentation of active_cell_iterators().
   *
   * @param[in] level A given level in the refinement hierarchy of this
   * triangulation.
   * @return The half open range <code>[this->begin_active(level),
   * this->end(level))</code>
   *
   * @pre level must be less than this->n_levels().
   *
   * @ingroup CPP11
   */
  IteratorRange<active_cell_iterator>
  active_cell_iterators_on_level(const unsigned int level) const;

  /*
   * @}
   */

  /*-------------------------------------------------------------------------*/

  /**
   * @name Face iterator functions
   * @{
   */

  /**
   * Iterator to the first used face.
   */
  face_iterator
  begin_face() const;

  /**
   * Iterator to the first active face.
   */
  active_face_iterator
  begin_active_face() const;

  /**
   * Iterator past the end; this iterator serves for comparisons of iterators
   * with past-the-end or before-the-beginning states.
   */
  face_iterator
  end_face() const;

  /**
   * Return an iterator range that contains all active faces that make up this
   * triangulation. This function is the face version of
   * Triangulation::active_cell_iterators(), and allows one to write code
   * like, e.g.,
   *
   * @code
   *   Triangulation<dim> triangulation;
   *   ...
   *   for (auto &face : triangulation.active_face_iterators())
   *     face->set_manifold_id(42);
   * @endcode
   *
   * @return The half open range <code>[this->begin_active_face(),
   * this->end_face())</code>
   *
   * @ingroup CPP11
   */
  IteratorRange<active_face_iterator>
  active_face_iterators() const;

  /*
   * @}
   */

  /*-------------------------------------------------------------------------*/

  /**
   * @name Vertex iterator functions
   * @{
   */

  /**
   * Iterator to the first used vertex. This function can only be used if dim
   * is not one.
   */
  vertex_iterator
  begin_vertex() const;

  /**
   * Iterator to the first active vertex. Because all vertices are active,
   * begin_vertex() and begin_active_vertex() return the same vertex. This
   * function can only be used if dim is not one.
   */
  active_vertex_iterator
  begin_active_vertex() const;

  /**
   * Iterator past the end; this iterator serves for comparisons of iterators
   * with past-the-end or before-the-beginning states. This function can only
   * be used if dim is not one.
   */
  vertex_iterator
  end_vertex() const;

  /*
   * @}
   */

  /**
   * @name Information about the triangulation
   * @{
   */

  /**
   * In the following, most functions are provided in two versions, with and
   * without an argument describing the level. The versions with this argument
   * are only applicable for objects describing the cells of the present
   * triangulation. For example: in 2D <tt>n_lines(level)</tt> cannot be
   * called, only <tt>n_lines()</tt>, as lines are faces in 2D and therefore
   * have no level.
   */

  /**
   * Return the total number of used lines, active or not.
   */
  unsigned int
  n_lines() const;

  /**
   * Return the total number of used lines, active or not on level @p level.
   */
  unsigned int
  n_lines(const unsigned int level) const;

  /**
   * Return the total number of active lines.
   */
  unsigned int
  n_active_lines() const;

  /**
   * Return the total number of active lines, on level @p level.
   */
  unsigned int
  n_active_lines(const unsigned int level) const;

  /**
   * Return the total number of used quads, active or not.
   */
  unsigned int
  n_quads() const;

  /**
   * Return the total number of used quads, active or not on level @p level.
   */
  unsigned int
  n_quads(const unsigned int level) const;

  /**
   * Return the total number of active quads, active or not.
   */
  unsigned int
  n_active_quads() const;

  /**
   * Return the total number of active quads, active or not on level @p level.
   */
  unsigned int
  n_active_quads(const unsigned int level) const;

  /**
   * Return the total number of used hexahedra, active or not.
   */
  unsigned int
  n_hexs() const;

  /**
   * Return the total number of used hexahedra, active or not on level @p
   * level.
   */
  unsigned int
  n_hexs(const unsigned int level) const;

  /**
   * Return the total number of active hexahedra, active or not.
   */
  unsigned int
  n_active_hexs() const;

  /**
   * Return the total number of active hexahedra, active or not on level @p
   * level.
   */
  unsigned int
  n_active_hexs(const unsigned int level) const;

  /**
   * Return the total number of used cells, active or not.  Maps to
   * <tt>n_lines()</tt> in one space dimension and so on.
   */
  unsigned int
  n_cells() const;

  /**
   * Return the total number of used cells, active or not, on level @p level.
   * Maps to <tt>n_lines(level)</tt> in one space dimension and so on.
   */
  unsigned int
  n_cells(const unsigned int level) const;

  /**
   * Return the total number of active cells. Maps to
   * <tt>n_active_lines()</tt> in one space dimension and so on.
   */
  unsigned int
  n_active_cells() const;

  /**
   * Return the total number of active cells. For the current class, this is
   * the same as n_active_cells(). However, the function may be overloaded in
   * derived classes (e.g., in parallel::distributed::Triangulation) where it
   * may return a value greater than the number of active cells reported by
   * the triangulation object on the current processor.
   */
  virtual types::global_dof_index
  n_global_active_cells() const;


  /**
   * Return the total number of active cells on level @p level.  Maps to
   * <tt>n_active_lines(level)</tt> in one space dimension and so on.
   */
  unsigned int
  n_active_cells(const unsigned int level) const;

  /**
   * Return the total number of used faces, active or not.  In 2D, the result
   * equals n_lines(), in 3D it equals n_quads(), while in 1D it equals
   * the number of used vertices.
   */
  unsigned int
  n_faces() const;

  /**
   * Return the total number of active faces.  In 2D, the result equals
   * n_active_lines(), in 3D it equals n_active_quads(), while in 1D it equals
   * the number of used vertices.
   */
  unsigned int
  n_active_faces() const;

  /**
   * Return the number of levels in this triangulation.
   *
   * @note Internally, triangulations store data in levels, and there may be
   * more levels in this data structure than one may think -- for example,
   * imagine a triangulation that we just got by coarsening the highest level
   * so that it was completely depopulated. That level is not removed, since
   * it will most likely be repopulated soon by the next refinement process.
   * As a consequence, if you happened to run through raw cell iterators
   * (which you can't do as a user of this class, but can internally), then
   * the number of objects in the levels hierarchy is larger than the level of
   * the most refined cell plus one. On the other hand, since this is rarely
   * what a user of this class cares about, the function really just returns
   * the level of the most refined active cell plus one. (The plus one is
   * because in a coarse, unrefined mesh, all cells have level zero -- making
   * the number of levels equal to one.)
   */
  unsigned int
  n_levels() const;

  /**
   * Return the number of levels in use. This function is equivalent to
   * n_levels() for a serial Triangulation, but gives the maximum of
   * n_levels() over all processors for a parallel::distributed::Triangulation
   * and therefore can be larger than n_levels().
   */
  virtual unsigned int
  n_global_levels() const;

  /**
   * Return true if the triangulation has hanging nodes.
   *
   * The function is made virtual since the result can be interpreted in
   * different ways, depending on whether the triangulation lives only on a
   * single processor, or may be distributed as done in the
   * parallel::distributed::Triangulation class (see there for a description
   * of what the function is supposed to do in the parallel context).
   */
  virtual bool
  has_hanging_nodes() const;

  /**
   * Return the total number of vertices.  Some of them may not be used, which
   * usually happens upon coarsening of a triangulation when some vertices are
   * discarded, but we do not want to renumber the remaining ones, leading to
   * holes in the numbers of used vertices.  You can get the number of used
   * vertices using @p n_used_vertices function.
   */
  unsigned int
  n_vertices() const;

  /**
   * Return a constant reference to all the vertices present in this
   * triangulation. Note that not necessarily all vertices in this array are
   * actually used; for example, if you coarsen a mesh, then some vertices are
   * deleted, but their positions in this array are unchanged as the indices
   * of vertices are only allocated once. You can find out about which
   * vertices are actually used by the function get_used_vertices().
   */
  const std::vector<Point<spacedim>> &
  get_vertices() const;

  /**
   * Return the number of vertices that are presently in use, i.e. belong to
   * at least one used element.
   */
  unsigned int
  n_used_vertices() const;

  /**
   * Return @p true if the vertex with this @p index is used.
   */
  bool
  vertex_used(const unsigned int index) const;

  /**
   * Return a constant reference to the array of @p bools indicating whether
   * an entry in the vertex array is used or not.
   */
  const std::vector<bool> &
  get_used_vertices() const;

  /**
   * Return the maximum number of cells meeting at a common vertex. Since this
   * number is an invariant under refinement, only the cells on the coarsest
   * level are considered. The operation is thus reasonably fast. The
   * invariance is only true for sufficiently many cells in the coarsest
   * triangulation (e.g. for a single cell one would be returned), so a
   * minimum of four is returned in two dimensions, 8 in three dimensions,
   * etc, which is how many cells meet if the triangulation is refined.
   *
   * In one space dimension, two is returned.
   */
  unsigned int
  max_adjacent_cells() const;

  /**
   * This function always returns @p invalid_subdomain_id but is there for
   * compatibility with the derived @p parallel::distributed::Triangulation
   * class. For distributed parallel triangulations this function returns the
   * subdomain id of those cells that are owned by the current processor.
   */
  virtual types::subdomain_id
  locally_owned_subdomain() const;

  /**
   * Return a reference to the current object.
   *
   * This doesn't seem to be very useful but allows to write code that can
   * access the underlying triangulation for anything that satisfies the
   * @ref ConceptMeshType "MeshType concept"
   * (which may not only be a triangulation, but also a DoFHandler, for
   * example).
   */
  Triangulation<dim, spacedim> &
  get_triangulation();

  /**
   * Return a reference to the current object. This is the const-version of
   * the previous function.
   */
  const Triangulation<dim, spacedim> &
  get_triangulation() const;


  /*
   * @}
   */

  /**
   * @name Internal information about the number of objects
   * @{
   */

  /**
   * Total number of lines, used or unused.
   *
   * @note This function really exports internal information about the
   * triangulation. It shouldn't be used in applications. The function is only
   * part of the public interface of this class because it is used in some of
   * the other classes that build very closely on it (in particular, the
   * DoFHandler class).
   */
  unsigned int
  n_raw_lines() const;

  /**
   * Number of lines, used or unused, on the given level.
   *
   * @note This function really exports internal information about the
   * triangulation. It shouldn't be used in applications. The function is only
   * part of the public interface of this class because it is used in some of
   * the other classes that build very closely on it (in particular, the
   * DoFHandler class).
   */
  unsigned int
  n_raw_lines(const unsigned int level) const;

  /**
   * Total number of quads, used or unused.
   *
   * @note This function really exports internal information about the
   * triangulation. It shouldn't be used in applications. The function is only
   * part of the public interface of this class because it is used in some of
   * the other classes that build very closely on it (in particular, the
   * DoFHandler class).
   */
  unsigned int
  n_raw_quads() const;

  /**
   * Number of quads, used or unused, on the given level.
   *
   * @note This function really exports internal information about the
   * triangulation. It shouldn't be used in applications. The function is only
   * part of the public interface of this class because it is used in some of
   * the other classes that build very closely on it (in particular, the
   * DoFHandler class).
   */
  unsigned int
  n_raw_quads(const unsigned int level) const;

  /**
   * Number of hexs, used or unused, on the given level.
   *
   * @note This function really exports internal information about the
   * triangulation. It shouldn't be used in applications. The function is only
   * part of the public interface of this class because it is used in some of
   * the other classes that build very closely on it (in particular, the
   * DoFHandler class).
   */
  unsigned int
  n_raw_hexs(const unsigned int level) const;

  /**
   * Number of cells, used or unused, on the given level.
   *
   * @note This function really exports internal information about the
   * triangulation. It shouldn't be used in applications. The function is only
   * part of the public interface of this class because it is used in some of
   * the other classes that build very closely on it (in particular, the
   * DoFHandler class).
   */
  unsigned int
  n_raw_cells(const unsigned int level) const;

  /**
   * Return the total number of faces, used or not. In 2d, the result equals
   * n_raw_lines(), in 3d it equals n_raw_quads(), while in 1D it equals
   * the number of vertices.
   *
   * @note This function really exports internal information about the
   * triangulation. It shouldn't be used in applications. The function is only
   * part of the public interface of this class because it is used in some of
   * the other classes that build very closely on it (in particular, the
   * DoFHandler class).
   */
  unsigned int
  n_raw_faces() const;

  /*
   * @}
   */

  /**
   * Determine an estimate for the memory consumption (in bytes) of this
   * object.
   *
   * This function is made virtual, since a triangulation object might be
   * accessed through a pointer to this base class, even if the actual object
   * is a derived class.
   */
  virtual std::size_t
  memory_consumption() const;

  /**
   * Write the data of this object to a stream for the purpose of
   * serialization.
   *
   * @note This function does not save <i>all</i> member variables of the
   * current triangulation. Rather, only certain kinds of information are
   * stored. For more information see the general documentation of this class.
   */
  template <class Archive>
  void
  save(Archive &ar, const unsigned int version) const;

  /**
   * Read the data of this object from a stream for the purpose of
   * serialization. Throw away the previous content.
   *
   * @note This function does not reset <i>all</i> member variables of the
   * current triangulation to the ones of the triangulation that was
   * previously stored to an archive. Rather, only certain kinds of
   * information are loaded. For more information see the general
   * documentation of this class.
   *
   * @note This function calls the Triangulation::clear() function and
   * consequently triggers the "clear" signal. After loading all data from the
   * archive, it then triggers the "create" signal. For more information on
   * signals, see the general documentation of this class.
   */
  template <class Archive>
  void
  load(Archive &ar, const unsigned int version);


  /**
   * Declare the (coarse) face pairs given in the argument of this function as
   * periodic. This way it is possible to obtain neighbors across periodic
   * boundaries.
   *
   * The vector can be filled by the function
   * GridTools::collect_periodic_faces.
   *
   * For more information on periodic boundary conditions see
   * GridTools::collect_periodic_faces, DoFTools::make_periodicity_constraints
   * and step-45.
   *
   * @note Before this function can be used the Triangulation has to be
   * initialized and must not be refined.
   */
  virtual void
  add_periodicity(
    const std::vector<GridTools::PeriodicFacePair<cell_iterator>> &);

  /**
   * Return the periodic_face_map.
   */
  const std::map<
    std::pair<cell_iterator, unsigned int>,
    std::pair<std::pair<cell_iterator, unsigned int>, std::bitset<3>>> &
  get_periodic_face_map() const;


  BOOST_SERIALIZATION_SPLIT_MEMBER()

  /**
   * @name Exceptions
   * @{
   */

  /**
   * Exception
   *
   * @ingroup Exceptions
   */
  DeclException2(ExcInvalidLevel,
                 int,
                 int,
                 << "You are requesting information from refinement level "
                 << arg1
                 << " of a triangulation, but this triangulation only has "
                 << arg2 << " refinement levels. The given level " << arg1
                 << " must be *less* than " << arg2 << ".");
  /**
   * The function raising this exception can only operate on an empty
   * Triangulation, i.e., a Triangulation without grid cells.
   *
   * @ingroup Exceptions
   */
  DeclException2(
    ExcTriangulationNotEmpty,
    int,
    int,
    << "You are trying to perform an operation on a triangulation "
    << "that is only allowed if the triangulation is currently empty. "
    << "However, it currently stores " << arg1 << " vertices and has "
    << "cells on " << arg2 << " levels.");
  /**
   * Trying to re-read a grid, an error occurred.
   *
   * @ingroup Exceptions
   */
  DeclException0(ExcGridReadError);
  /**
   * Exception
   * @ingroup Exceptions
   */
  DeclException0(ExcFacesHaveNoLevel);
  /**
   * The triangulation level you accessed is empty.
   *
   * @ingroup Exceptions
   */
  DeclException1(ExcEmptyLevel,
                 int,
                 << "You tried to do something on level " << arg1
                 << ", but this level is empty.");
  /**
   * Exception
   *
   * @ingroup Exceptions
   */
  DeclException0(ExcNonOrientableTriangulation);

  /**
   * Exception
   *
   * Requested boundary_id not found
   *
   * @ingroup Exceptions
   */
  DeclException1(ExcBoundaryIdNotFound,
                 types::boundary_id,
                 << "The given boundary_id " << arg1
                 << " is not defined in this Triangulation!");

  /**
   * Exception
   *
   * @ingroup Exceptions
   */
  DeclExceptionMsg(
    ExcInconsistentCoarseningFlags,
    "A cell is flagged for coarsening, but either not all of its siblings "
    "are active or flagged for coarsening as well. Please clean up all "
    "coarsen flags on your triangulation via "
    "Triangulation::prepare_coarsening_and_refinement() beforehand!");

  /*
   * @}
   */

protected:
  /**
   * Do some smoothing in the process of refining the triangulation. See the
   * general doc of this class for more information about this.
   */
  MeshSmoothing smooth_grid;

  /**
   * Write a bool vector to the given stream, writing a pre- and a postfix
   * magic number. The vector is written in an almost binary format, i.e. the
   * bool flags are packed but the data is written as ASCII text.
   *
   * The flags are stored in a binary format: for each @p true, a @p 1 bit is
   * stored, a @p 0 bit otherwise.  The bits are stored as <tt>unsigned
   * char</tt>, thus avoiding endianness. They are written to @p out in plain
   * text, thus amounting to 3.6 bits in the output per bits in the input on
   * the average. Other information (magic numbers and number of elements of
   * the input vector) is stored as plain text as well. The format should
   * therefore be interplatform compatible.
   */
  static void
  write_bool_vector(const unsigned int       magic_number1,
                    const std::vector<bool> &v,
                    const unsigned int       magic_number2,
                    std::ostream &           out);

  /**
   * Re-read a vector of bools previously written by @p write_bool_vector and
   * compare with the magic numbers.
   */
  static void
  read_bool_vector(const unsigned int magic_number1,
                   std::vector<bool> &v,
                   const unsigned int magic_number2,
                   std::istream &     in);

  /**
   * Recreate information about periodic neighbors from
   * periodic_face_pairs_level_0.
   */
  void
  update_periodic_face_map();


private:
  /**
   * If add_periodicity() is called, this variable stores the given periodic
   * face pairs on level 0 for later access during the identification of ghost
   * cells for the multigrid hierarchy and for setting up the
   * periodic_face_map.
   */
  std::vector<GridTools::PeriodicFacePair<cell_iterator>>
    periodic_face_pairs_level_0;

  /**
   * If add_periodicity() is called, this variable stores the active periodic
   * face pairs.
   */
  std::map<std::pair<cell_iterator, unsigned int>,
           std::pair<std::pair<cell_iterator, unsigned int>, std::bitset<3>>>
    periodic_face_map;

  /**
   * @name Cell iterator functions for internal use
   * @{
   */

  /**
   * Declare a number of iterator types for raw iterators, i.e., iterators
   * that also iterate over holes in the list of cells left by cells that have
   * been coarsened away in previous mesh refinement cycles.
   *
   * Since users should never have to access these internal properties of how
   * we store data, these iterator types are made private.
   */
  using raw_cell_iterator = TriaRawIterator<CellAccessor<dim, spacedim>>;
  using raw_face_iterator =
    TriaRawIterator<TriaAccessor<dim - 1, dim, spacedim>>;
  using raw_vertex_iterator =
    TriaRawIterator<dealii::TriaAccessor<0, dim, spacedim>>;
  using raw_line_iterator = typename IteratorSelector::raw_line_iterator;
  using raw_quad_iterator = typename IteratorSelector::raw_quad_iterator;
  using raw_hex_iterator  = typename IteratorSelector::raw_hex_iterator;

  /**
   * Iterator to the first cell, used or not, on level @p level. If a level
   * has no cells, a past-the-end iterator is returned.
   */
  raw_cell_iterator
  begin_raw(const unsigned int level = 0) const;

  /**
   * Return a raw iterator which is the first iterator not on level. If @p
   * level is the last level, then this returns <tt>end()</tt>.
   */
  raw_cell_iterator
  end_raw(const unsigned int level) const;

  /*
   * @}
   */

  /**
   * @name Line iterator functions for internal use
   * @{
   */

  /**
   * Iterator to the first line, used or not, on level @p level. If a level
   * has no lines, a past-the-end iterator is returned.  If lines are no
   * cells, i.e. for @p dim>1 no @p level argument must be given.  The same
   * applies for all the other functions above, of course.
   */
  raw_line_iterator
  begin_raw_line(const unsigned int level = 0) const;

  /**
   * Iterator to the first used line on level @p level.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  line_iterator
  begin_line(const unsigned int level = 0) const;

  /**
   * Iterator to the first active line on level @p level.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  active_line_iterator
  begin_active_line(const unsigned int level = 0) const;

  /**
   * Iterator past the end; this iterator serves for comparisons of iterators
   * with past-the-end or before-the-beginning states.
   */
  line_iterator
  end_line() const;

  /*
   * @}
   */

  /**
   * @name Quad iterator functions for internal use
   * @{
   */

  /**
   * Iterator to the first quad, used or not, on the given level. If a level
   * has no quads, a past-the-end iterator is returned.  If quads are no
   * cells, i.e. for $dim>2$ no level argument must be given.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  raw_quad_iterator
  begin_raw_quad(const unsigned int level = 0) const;

  /**
   * Iterator to the first used quad on level @p level.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  quad_iterator
  begin_quad(const unsigned int level = 0) const;

  /**
   * Iterator to the first active quad on level @p level.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  active_quad_iterator
  begin_active_quad(const unsigned int level = 0) const;

  /**
   * Iterator past the end; this iterator serves for comparisons of iterators
   * with past-the-end or before-the-beginning states.
   */
  quad_iterator
  end_quad() const;

  /*
   * @}
   */

  /**
   * @name Hex iterator functions for internal use
   * @{
   */

  /**
   * Iterator to the first hex, used or not, on level @p level. If a level has
   * no hexes, a past-the-end iterator is returned.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  raw_hex_iterator
  begin_raw_hex(const unsigned int level = 0) const;

  /**
   * Iterator to the first used hex on level @p level.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  hex_iterator
  begin_hex(const unsigned int level = 0) const;

  /**
   * Iterator to the first active hex on level @p level.
   *
   * @note The given @p level argument needs to correspond to a level of the
   *   triangulation, i.e., should be less than the value returned by
   *   n_levels(). On the other hand, for parallel computations using
   *   a parallel::distributed::Triangulation object, it is often convenient
   *   to write loops over the cells of all levels of the global mesh, even
   *   if the <i>local</i> portion of the triangulation does not actually
   *   have cells at one of the higher levels. In those cases, the
   *   @p level argument is accepted if it is less than what the
   *   n_global_levels() function returns. If the given @p level is
   *   between the values returned by n_levels() and n_global_levels(),
   *   then no cells exist in the local portion of the triangulation
   *   at this level, and the function simply returns what end() would
   *   return.
   */
  active_hex_iterator
  begin_active_hex(const unsigned int level = 0) const;

  /**
   * Iterator past the end; this iterator serves for comparisons of iterators
   * with past-the-end or before-the-beginning states.
   */
  hex_iterator
  end_hex() const;

  /*
   * @}
   */


  /**
   * The (public) function clear() will only work when the triangulation is
   * not subscribed to by other users. The clear_despite_subscriptions()
   * function now allows the triangulation being cleared even when there are
   * subscriptions.
   *
   * Make sure, you know what you do, when calling this function, as its use
   * is reasonable in very rare cases, only. For example, when the
   * subscriptions were for the initially empty Triangulation and the
   * Triangulation object wants to release its memory before throwing an
   * assertion due to input errors (e.g. in the create_triangulation()
   * function).
   */
  void
  clear_despite_subscriptions();

  /**
   * For all cells, set the active cell indices so that active cells know the
   * how many-th active cell they are, and all other cells have an invalid
   * value. This function is called after mesh creation, refinement, and
   * serialization.
   */
  void
  reset_active_cell_indices();

  /**
   * Refine all cells on all levels which were previously flagged for
   * refinement.
   *
   * Note, that this function uses the <tt>line->user_flags</tt> for
   * <tt>dim=2,3</tt> and the <tt>quad->user_flags</tt> for <tt>dim=3</tt>.
   *
   * The function returns a list of cells that have produced children that
   * satisfy the criteria of
   * @ref GlossDistorted "distorted cells"
   * if the <code>check_for_distorted_cells</code> flag was specified upon
   * creation of this object, at
   */
  DistortedCellList
  execute_refinement();

  /**
   * Coarsen all cells which were flagged for coarsening, or rather: delete
   * all children of those cells of which all child cells are flagged for
   * coarsening and several other constraints hold (see the general doc of
   * this class).
   */
  void
  execute_coarsening();

  /**
   * Make sure that either all or none of the children of a cell are tagged
   * for coarsening.
   */
  void
  fix_coarsen_flags();

  /**
   * Array of pointers pointing to the objects storing the cell data on the
   * different levels.
   */
  std::vector<std::unique_ptr<
    dealii::internal::TriangulationImplementation::TriaLevel<dim>>>
    levels;

  /**
   * Pointer to the faces of the triangulation. In 1d this contains nothing,
   * in 2D it contains data concerning lines and in 3D quads and lines.  All
   * of these have no level and are therefore treated separately.
   */
  std::unique_ptr<dealii::internal::TriangulationImplementation::TriaFaces<dim>>
    faces;


  /**
   * Array of the vertices of this triangulation.
   */
  std::vector<Point<spacedim>> vertices;

  /**
   * Array storing a bit-pattern which vertices are used.
   */
  std::vector<bool> vertices_used;

  /**
   * Collection of manifold objects. We store only objects, which are not of
   * type FlatManifold.
   */
  std::map<types::manifold_id, std::unique_ptr<const Manifold<dim, spacedim>>>
    manifold;

  /**
   * Flag indicating whether anisotropic refinement took place.
   */
  bool anisotropic_refinement;


  /**
   * A flag that determines whether we are to check for distorted cells upon
   * creation and refinement of a mesh.
   */
  const bool check_for_distorted_cells;

  /**
   * Cache to hold the numbers of lines, quads, hexes, etc. These numbers are
   * set at the end of the refinement and coarsening functions and enable
   * faster access later on. In the old days, whenever one wanted to access
   * one of these numbers, one had to perform a loop over all lines, e.g., and
   * count the elements until we hit the end iterator. This is time consuming
   * and since access to the number of lines etc is a rather frequent
   * operation, this was not an optimal solution.
   */
  dealii::internal::TriangulationImplementation::NumberCache<dim> number_cache;

  /**
   * A map that relates the number of a boundary vertex to the boundary
   * indicator. This field is only used in 1d. We have this field because we
   * store boundary indicator information with faces in 2d and higher where we
   * have space in the structures that store data for faces, but in 1d there
   * is no such space for faces.
   *
   * The field is declared as a pointer for a rather mundane reason: all other
   * fields of this class that can be modified by the TriaAccessor hierarchy
   * are pointers, and so these accessor classes store a const pointer to the
   * triangulation. We could no longer do so for TriaAccessor<0,1,spacedim> if
   * this field (that can be modified by TriaAccessor::set_boundary_id) were
   * not a pointer.
   */
  std::unique_ptr<std::map<unsigned int, types::boundary_id>>
    vertex_to_boundary_id_map_1d;


  /**
   * A map that relates the number of a boundary vertex to the manifold
   * indicator. This field is only used in 1d. We have this field because we
   * store manifold indicator information with faces in 2d and higher where we
   * have space in the structures that store data for faces, but in 1d there
   * is no such space for faces.
   *
   * @note Manifold objects are pretty useless for points since they are
   * neither refined nor are their interiors mapped. We nevertheless allow
   * storing manifold ids for points to be consistent in dimension-independent
   * programs.
   *
   * The field is declared as a pointer for a rather mundane reason: all other
   * fields of this class that can be modified by the TriaAccessor hierarchy
   * are pointers, and so these accessor classes store a const pointer to the
   * triangulation. We could no longer do so for TriaAccessor<0,1,spacedim> if
   * this field (that can be modified by TriaAccessor::set_manifold_id) were
   * not a pointer.
   */
  std::unique_ptr<std::map<unsigned int, types::manifold_id>>
    vertex_to_manifold_id_map_1d;

  // make a couple of classes friends
  template <int, int, int>
  friend class TriaAccessorBase;
  template <int, int, int>
  friend class TriaAccessor;
  friend class TriaAccessor<0, 1, spacedim>;

  friend class CellAccessor<dim, spacedim>;

  friend struct dealii::internal::TriaAccessorImplementation::Implementation;

  friend class hp::DoFHandler<dim, spacedim>;

  friend struct dealii::internal::TriangulationImplementation::Implementation;

  template <typename>
  friend class dealii::internal::TriangulationImplementation::TriaObjects;

  // explicitly check for sensible template arguments, but not on windows
  // because MSVC creates bogus warnings during normal compilation
#ifndef DEAL_II_MSVC
  static_assert(dim <= spacedim,
                "The dimension <dim> of a Triangulation must be less than or "
                "equal to the space dimension <spacedim> in which it lives.");
#endif
};


#ifndef DOXYGEN


template <int structdim>
inline CellData<structdim>::CellData()
{
  for (unsigned int i = 0; i < GeometryInfo<structdim>::vertices_per_cell; ++i)
    vertices[i] = numbers::invalid_unsigned_int;

  material_id = 0;

  // And the manifold to be invalid
  manifold_id = numbers::flat_manifold_id;
}



namespace internal
{
  namespace TriangulationImplementation
  {
    template <class Archive>
    void
    NumberCache<1>::serialize(Archive &ar, const unsigned int)
    {
      ar &n_levels;
      ar &n_lines &n_lines_level;
      ar &n_active_lines &n_active_lines_level;
    }


    template <class Archive>
    void
    NumberCache<2>::serialize(Archive &ar, const unsigned int version)
    {
      this->NumberCache<1>::serialize(ar, version);

      ar &n_quads &n_quads_level;
      ar &n_active_quads &n_active_quads_level;
    }


    template <class Archive>
    void
    NumberCache<3>::serialize(Archive &ar, const unsigned int version)
    {
      this->NumberCache<2>::serialize(ar, version);

      ar &n_hexes &n_hexes_level;
      ar &n_active_hexes &n_active_hexes_level;
    }

  } // namespace TriangulationImplementation
} // namespace internal


template <int dim, int spacedim>
inline bool
Triangulation<dim, spacedim>::vertex_used(const unsigned int index) const
{
  Assert(index < vertices_used.size(),
         ExcIndexRange(index, 0, vertices_used.size()));
  return vertices_used[index];
}



template <int dim, int spacedim>
inline unsigned int
Triangulation<dim, spacedim>::n_levels() const
{
  return number_cache.n_levels;
}

template <int dim, int spacedim>
inline unsigned int
Triangulation<dim, spacedim>::n_global_levels() const
{
  return number_cache.n_levels;
}


template <int dim, int spacedim>
inline unsigned int
Triangulation<dim, spacedim>::n_vertices() const
{
  return vertices.size();
}



template <int dim, int spacedim>
inline const std::vector<Point<spacedim>> &
Triangulation<dim, spacedim>::get_vertices() const
{
  return vertices;
}


template <int dim, int spacedim>
template <class Archive>
void
Triangulation<dim, spacedim>::save(Archive &ar, const unsigned int) const
{
  // as discussed in the documentation, do not store the signals as
  // well as boundary and manifold description but everything else
  ar &smooth_grid;

  unsigned int n_levels = levels.size();
  ar &         n_levels;
  for (unsigned int i = 0; i < levels.size(); ++i)
    ar &levels[i];

  // boost dereferences a nullptr when serializing a nullptr
  // at least up to 1.65.1. This causes problems with clang-5.
  // Therefore, work around it.
  bool faces_is_nullptr = (faces.get() == nullptr);
  ar & faces_is_nullptr;
  if (!faces_is_nullptr)
    ar &faces;

  ar &vertices;
  ar &vertices_used;

  ar &anisotropic_refinement;
  ar &number_cache;

  ar &check_for_distorted_cells;

  if (dim == 1)
    {
      ar &vertex_to_boundary_id_map_1d;
      ar &vertex_to_manifold_id_map_1d;
    }
}



template <int dim, int spacedim>
template <class Archive>
void
Triangulation<dim, spacedim>::load(Archive &ar, const unsigned int)
{
  // clear previous content. this also calls the respective signal
  clear();

  // as discussed in the documentation, do not store the signals as
  // well as boundary and manifold description but everything else
  ar &smooth_grid;

  unsigned int size;
  ar &         size;
  levels.resize(size);
  for (unsigned int i = 0; i < levels.size(); ++i)
    {
      std::unique_ptr<internal::TriangulationImplementation::TriaLevel<dim>>
          level;
      ar &level;
      levels[i] = std::move(level);
    }

  // Workaround for nullptr, see in save().
  bool faces_is_nullptr = true;
  ar & faces_is_nullptr;
  if (!faces_is_nullptr)
    ar &faces;

  ar &vertices;
  ar &vertices_used;

  ar &anisotropic_refinement;
  ar &number_cache;

  // the levels do not serialize the active_cell_indices because
  // they are easy enough to rebuild upon re-loading data. do
  // this here. don't forget to first resize the fields appropriately
  {
    for (unsigned int l = 0; l < levels.size(); ++l)
      levels[l]->active_cell_indices.resize(levels[l]->refine_flags.size());
    reset_active_cell_indices();
  }


  bool my_check_for_distorted_cells;
  ar & my_check_for_distorted_cells;

  Assert(my_check_for_distorted_cells == check_for_distorted_cells,
         ExcMessage("The triangulation loaded into here must have the "
                    "same setting with regard to reporting distorted "
                    "cell as the one previously stored."));

  if (dim == 1)
    {
      ar &vertex_to_boundary_id_map_1d;
      ar &vertex_to_manifold_id_map_1d;
    }

  // trigger the create signal to indicate
  // that new content has been imported into
  // the triangulation
  signals.create();
}


/* -------------- declaration of explicit specializations ------------- */

template <>
unsigned int
Triangulation<1, 1>::n_raw_lines(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 1>::n_quads() const;
template <>
unsigned int
Triangulation<1, 1>::n_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 1>::n_raw_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<2, 2>::n_raw_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 1>::n_raw_hexs(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 1>::n_active_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 1>::n_active_quads() const;
template <>
unsigned int
Triangulation<1, 1>::max_adjacent_cells() const;


// -------------------------------------------------------------------
// -- Explicit specializations for codimension one grids


template <>
unsigned int
Triangulation<1, 2>::n_raw_lines(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 2>::n_quads() const;
template <>
unsigned int
Triangulation<1, 2>::n_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 2>::n_raw_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<2, 3>::n_raw_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 2>::n_raw_hexs(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 2>::n_active_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 2>::n_active_quads() const;
template <>
unsigned int
Triangulation<1, 2>::max_adjacent_cells() const;

// -------------------------------------------------------------------
// -- Explicit specializations for codimension two grids


template <>
unsigned int
Triangulation<1, 3>::n_raw_lines(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 3>::n_quads() const;
template <>
unsigned int
Triangulation<1, 3>::n_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 3>::n_raw_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<2, 3>::n_raw_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 3>::n_raw_hexs(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 3>::n_active_quads(const unsigned int level) const;
template <>
unsigned int
Triangulation<1, 3>::n_active_quads() const;
template <>
unsigned int
Triangulation<1, 3>::max_adjacent_cells() const;


#endif // DOXYGEN

DEAL_II_NAMESPACE_CLOSE

// Include tria_accessor.h here, so that it is possible for an end
// user to use the iterators of Triangulation<dim> directly without
// the need to include tria_accessor.h separately. (Otherwise the
// iterators are an 'opaque' or 'incomplete' type.)
#include <deal.II/grid/tria_accessor.h>

#endif
