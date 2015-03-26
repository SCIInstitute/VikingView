#include <Data/Structure.h>
#include <Data/Json.h>
#include <Data/PointSampler.h>
#include <Data/AlphaShape.h>
#include <Data/FixedAlphaShape.h>

#include <vtkCenterOfMass.h>
#include <vtkMassProperties.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkSTLWriter.h>
#include <vtkFeatureEdges.h>
#include <vtkCellArray.h>
#include <vtkFillHolesFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkTriangleFilter.h>
//#include <vtkFeatureVertices.h>
#include <vtkLoopSubdivisionFilter.h>
#include <vtkLinearSubdivisionFilter.h>
#include <vtkLineSource.h>
#include <vtkWindowedSincPolyDataFilter.h>
//#include <vtkPLYWriter.h>

#include <vtkPolyDataWriter.h>
#include <vtkBooleanOperationPolyDataFilter.h>
#include <vtkAppendPolyData.h>
#include <vtkTubeFilter.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>

#include <vtkSphereSource.h>
#include <vtkTriangle.h>
#include <vtkMath.h>
#include <vtkDecimatePro.h>
#include <vtkTupleInterpolator.h>

#include <vtkParametricSpline.h>
#include <vtkParametricFunctionSource.h>

#include <Visualization/customQuadricDecimation.h>

#include <vtkButterflySubdivisionFilter.h>

#include <CGAL/IO/Polyhedron_iostream.h>
#include <CGAL/Inverse_index.h>
#include <CGAL/make_skin_surface_mesh_3.h>

//#include <CGAL/Polyhe>

//-----------------------------------------------------------------------------
Structure::Structure()
{
  this->color_ = QColor( 128 + ( qrand() % 128 ), 128 + ( qrand() % 128 ), 128 + ( qrand() % 128 ) );
  this->num_tubes_ = 0;
}

//-----------------------------------------------------------------------------
Structure::~Structure()
{}

//-----------------------------------------------------------------------------
QSharedPointer<Structure> Structure::create_structure( int id, QList<QVariant> structure_list,
                                                       QList<QVariant> location_list, QList<QVariant> link_list )
{

  QSharedPointer<Structure> structure = QSharedPointer<Structure>( new Structure() );
  structure->id_ = id;

  float units_per_pixel = 2.18 / 1000.0;
  float units_per_section = -( 90.0 / 1000.0 );

  std::cerr << "structure list length: " << structure_list.size() << "\n";
  std::cerr << "location list length: " << location_list.size() << "\n";
  std::cerr << "link list length: " << link_list.size() << "\n";

  // construct nodes
  foreach( QVariant var, location_list ) {
    Node n;
    QMap<QString, QVariant> item = var.toMap();
    n.x = item["VolumeX"].toDouble();
    n.y = item["VolumeY"].toDouble();
    n.z = item["Z"].toDouble();
    n.radius = item["Radius"].toDouble();
    n.id = item["ID"].toLongLong();
    n.graph_id = -1;

    if ( n.z == 56 || n.z == 8 || n.z == 22 || n.z == 81 || n.z == 72 || n.z == 60 )
    {
      continue;
    }

    // scale
    n.x = n.x * units_per_pixel;
    n.y = n.y * units_per_pixel;
    n.z = n.z * units_per_section;
    n.radius = n.radius * units_per_pixel;

    structure->node_map_[n.id] = n;
  }

  std::cerr << "Found " << structure->node_map_.size() << " nodes\n";

  foreach( QVariant var, link_list ) {
    Link link;
    QMap<QString, QVariant> item = var.toMap();

    link.a = item["A"].toLongLong();
    link.b = item["B"].toLongLong();

    if ( structure->node_map_.find( link.a ) == structure->node_map_.end()
         || structure->node_map_.find( link.b ) == structure->node_map_.end() )
    {
      continue;
    }

    structure->node_map_[link.a].linked_nodes.append( link.b );
    structure->node_map_[link.b].linked_nodes.append( link.a );
    structure->links_.append( link );
  }

  std::cerr << "Found " << structure->links_.size() << " links\n";

  std::cerr << "===Initial===\n";
  structure->link_report();

  structure->connect_subgraphs();

  std::cerr << "===After graph connection===\n";
  structure->link_report();

  std::cerr << "number of nodes : " << structure->node_map_.size() << "\n";

  structure->cull_locations();

  structure->connect_subgraphs();

  std::cerr << "===After location culling===\n";
  structure->link_report();
  return structure;
}

//-----------------------------------------------------------------------------
NodeMap Structure::get_node_map()
{
  return this->node_map_;
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> Structure::get_mesh_old()
{
  if ( this->mesh_ )
  {
    return this->mesh_;
  }

  NodeMap node_map = this->get_node_map();

  std::list<Point> points;

  vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();

  bool first = true;

  // spheres
  for ( NodeMap::iterator it = node_map.begin(); it != node_map.end(); ++it )
  {

    Node n = it->second;

    if ( n.linked_nodes.size() != 1 )
    {
      continue;
    }

//    std::cerr << "adding sphere: " << n.id << "(" << n.x << "," << n.y << "," << n.z << "," << n.radius << ")\n";

    vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetCenter( n.x, n.y, n.z );
    sphere->SetRadius( n.radius );
    sphere->Update();

    if ( first )
    {
      poly_data = sphere->GetOutput();
      first = false;
    }
    else
    {

      vtkSmartPointer<vtkBooleanOperationPolyDataFilter> booleanOperation =
        vtkSmartPointer<vtkBooleanOperationPolyDataFilter>::New();
      booleanOperation->SetOperationToUnion();

      booleanOperation->SetInputData( 0, poly_data );
      booleanOperation->SetInputData( 1, sphere->GetOutput() );
      booleanOperation->Update();
      poly_data = booleanOperation->GetOutput();

/*
   vtkSmartPointer<vtkAppendPolyData> append = vtkSmartPointer<vtkAppendPolyData>::New();
   append->AddInputData( poly_data );
   append->AddInputData( sphere->GetOutput() );
   append->Update();
   poly_data = append->GetOutput();
 */
    }
  }

/*

      vtkSmartPointer<vtkBooleanOperationPolyDataFilter> booleanOperation =
        vtkSmartPointer<vtkBooleanOperationPolyDataFilter>::New();
      booleanOperation->SetOperationToUnion();

      booleanOperation->SetInputData( 0, poly_data );
      booleanOperation->SetInputData( 1, poly_data );
      booleanOperation->Update();
      poly_data = booleanOperation->GetOutput();
 */

  foreach( Link link, this->get_links() ) {

    if ( node_map.find( link.a ) == node_map.end() || node_map.find( link.b ) == node_map.end() )
    {
      continue;
    }

    Node n1 = node_map[link.a];
    Node n2 = node_map[link.b];

    vtkSmartPointer<vtkPoints> vtk_points = vtkSmartPointer<vtkPoints>::New();

    vtk_points->InsertNextPoint( n1.x, n1.y, n1.z );
    vtk_points->InsertNextPoint( n2.x, n2.y, n2.z );

    vtkSmartPointer<vtkCellArray> lines =
      vtkSmartPointer<vtkCellArray>::New();
    lines->InsertNextCell( 2 );
    lines->InsertCellPoint( 0 );
    lines->InsertCellPoint( 1 );

    vtkSmartPointer<vtkPolyData> polyData =
      vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints( vtk_points );
    polyData->SetLines( lines );

    vtkSmartPointer<vtkTubeFilter> tube
      = vtkSmartPointer<vtkTubeFilter>::New();
    tube->SetInputData( polyData );
    tube->CappingOn();
    tube->SetRadius( n1.radius );
    tube->SetNumberOfSides( 10 );
    tube->Update();

    vtkSmartPointer<vtkAppendPolyData> append = vtkSmartPointer<vtkAppendPolyData>::New();
    append->AddInputData( poly_data );
    append->AddInputData( tube->GetOutput() );
    append->Update();
    poly_data = append->GetOutput();
  }

  this->mesh_ = poly_data;

  return this->mesh_;
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> Structure::get_mesh_alpha()
{
  if ( !this->mesh_ )
  {
    PointSampler ps( this );
    std::list<Point> points = ps.sample_points();

    AlphaShape alpha_shape;
    alpha_shape.set_points( points );
    vtkSmartPointer<vtkPolyData> poly_data = alpha_shape.get_mesh();

    //FixedAlphaShape alpha_shape;
    //alpha_shape.set_points( points );
    //vtkSmartPointer<vtkPolyData> poly_data = alpha_shape.get_mesh();

    // clean
    std::cerr << "Number of points before cleaning: " << poly_data->GetNumberOfPoints() << "\n";
    vtkSmartPointer<vtkCleanPolyData> clean = vtkSmartPointer<vtkCleanPolyData>::New();
    clean->SetInputData( poly_data );
    clean->SetTolerance( 0.00001 );
    clean->Update();
    poly_data = clean->GetOutput();
    std::cerr << "Number of points after cleaning: " << poly_data->GetNumberOfPoints() << "\n";

    vtkSmartPointer<vtkFeatureEdges> features = vtkSmartPointer<vtkFeatureEdges>::New();
    features->SetInputData( poly_data );
    features->NonManifoldEdgesOn();
    features->BoundaryEdgesOff();
    features->FeatureEdgesOff();
    features->Update();

    vtkSmartPointer<vtkPolyData> nonmanifold = features->GetOutput();

    std::cerr << "Number of non-manifold points: " << nonmanifold->GetNumberOfPoints() << "\n";
    std::cerr << "Number of non-manifold cells: " << nonmanifold->GetNumberOfCells() << "\n";

    std::vector<int> remove;

    for ( int j = 0; j < poly_data->GetNumberOfPoints(); j++ )
    {
      double p2[3];
      poly_data->GetPoint( j, p2 );

      for ( int i = 0; i < nonmanifold->GetNumberOfPoints(); i++ )
      {
        double p[3];
        nonmanifold->GetPoint( i, p );

        if ( p[0] == p2[0] && p[1] == p2[1] && p[2] == p2[2] )
        {
          remove.push_back( j );
        }
      }
    }

    std::cerr << "Removing " << remove.size() << " non-manifold vertices\n";

    vtkSmartPointer<vtkPolyData> new_poly_data = vtkSmartPointer<vtkPolyData>::New();
    vtkSmartPointer<vtkPoints> vtk_pts = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> vtk_triangles = vtkSmartPointer<vtkCellArray>::New();

    for ( int i = 0; i < poly_data->GetNumberOfCells(); i++ )
    {
      vtkSmartPointer<vtkIdList> list = vtkIdList::New();
      poly_data->GetCellPoints( i, list );

      bool match = false;
      for ( int j = 0; j < list->GetNumberOfIds(); j++ )
      {
        int id = list->GetId( j );
        for ( unsigned int k = 0; k < remove.size(); k++ )
        {
          if ( id == remove[k] )
          {
            match = true;
          }
        }
      }

      if ( match )
      {
        poly_data->DeleteCell( i );
      }
    }

    poly_data->RemoveDeletedCells();

    features = vtkSmartPointer<vtkFeatureEdges>::New();
    features->SetInputData( poly_data );
    features->NonManifoldEdgesOn();
    features->BoundaryEdgesOff();
    features->FeatureEdgesOff();
    features->Update();

    nonmanifold = features->GetOutput();

    //std::cerr < "Number of non-manifold points: " << nonmanifold->GetNumberOfPoints() << "\n";
    //std::cerr << "Number of non-manifold cells: " << nonmanifold->GetNumberOfCells() << "\n";

    vtkSmartPointer< vtkTriangleFilter > triangle_filter =
      vtkSmartPointer< vtkTriangleFilter >::New();
    triangle_filter->SetInputData( poly_data );
    triangle_filter->Update();
    poly_data = triangle_filter->GetOutput();

    vtkSmartPointer<vtkSTLWriter> writer = vtkSmartPointer<vtkSTLWriter>::New();
    writer->SetFileName( "Z:\\shared\\file.stl" );
    writer->SetInputData( poly_data );
    writer->Write();

/*
   // fill holes
   vtkSmartPointer<vtkFillHolesFilter> fill_holes = vtkSmartPointer<vtkFillHolesFilter>::New();
   fill_holes->SetInputData( poly_data );
   fill_holes->SetHoleSize( 300 );
   fill_holes->Update();
   poly_data = fill_holes->GetOutput();



 */

    // clean
    clean = vtkSmartPointer<vtkCleanPolyData>::New();
    clean->SetInputData( poly_data );
    clean->Update();
    poly_data = clean->GetOutput();

    // smooth

/*

    vtkSmartPointer<vtkWindowedSincPolyDataFilter> smooth = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
    smooth->SetInputData( poly_data );
    smooth->SetPassBand( 0.1 );
    smooth->SetNumberOfIterations( 200 );
    smooth->FeatureEdgeSmoothingOn();
    smooth->NonManifoldSmoothingOn();
    smooth->Update();
    poly_data = smooth->GetOutput();
 */

    vtkSmartPointer<vtkLoopSubdivisionFilter> subdivision = vtkSmartPointer<vtkLoopSubdivisionFilter>::New();
    subdivision->SetInputData( poly_data );
    subdivision->SetNumberOfSubdivisions( 2 );
    subdivision->Update();
    poly_data = subdivision->GetOutput();

    // Make the triangle winding order consistent
    vtkSmartPointer<vtkPolyDataNormals> normals =
      vtkSmartPointer<vtkPolyDataNormals>::New();
    normals->SetInputData( poly_data );
    normals->ConsistencyOn();
    normals->SplittingOff();
    normals->Update();
    poly_data = normals->GetOutput();

/*
    vtkSmartPointer<vtkSmoothPolyDataFilter> smooth = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
    smooth->SetInputData( poly_data );
    smooth->SetNumberOfIterations( 200 );
    smooth->Update();
    poly_data = smooth->GetOutput();
 */

/*
    vtkSmartPointer<vtkWindowedSincPolyDataFilter> smooth = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
    smooth->SetInputData( poly_data );
    smooth->SetNumberOfIterations( 200 );
    smooth->SetPassBand(0.20);
    smooth->Update();
    poly_data = smooth->GetOutput();
 */

    // Make the triangle winding order consistent
    normals = vtkSmartPointer<vtkPolyDataNormals>::New();
    normals->SetInputData( poly_data );
    normals->ConsistencyOn();
    normals->SplittingOff();
    normals->Update();
    poly_data = normals->GetOutput();

    this->mesh_ = poly_data;
  }

  return this->mesh_;
}

//-----------------------------------------------------------------------------
int Structure::get_id()
{
  return this->id_;
}

//-----------------------------------------------------------------------------
double Structure::get_volume()
{
  return 0;
  vtkSmartPointer<vtkPolyData> mesh = this->get_mesh_union();

  vtkSmartPointer<vtkMassProperties> mass_properties = vtkSmartPointer<vtkMassProperties>::New();

  mass_properties->SetInputData( mesh );
  mass_properties->Update();

  return mass_properties->GetVolume();
}

//-----------------------------------------------------------------------------
QString Structure::get_center_of_mass_string()
{
  return "";
  vtkSmartPointer<vtkPolyData> mesh = this->get_mesh_union();

  // Compute the center of mass
  vtkSmartPointer<vtkCenterOfMass> center_of_mass =
    vtkSmartPointer<vtkCenterOfMass>::New();
  center_of_mass->SetInputData( mesh );
  center_of_mass->SetUseScalarsAsWeights( false );
  center_of_mass->Update();

  double center[3];
  center_of_mass->GetCenter( center );

  QString str = QString::number( center[0] ) + ", " + QString::number( center[1] ) + ", " + QString::number( center[2] );

  return str;
}

//-----------------------------------------------------------------------------
QList<Link> Structure::get_links()
{
  return this->links_;
}

//-----------------------------------------------------------------------------
void Structure::set_color( QColor color )
{
  this->color_ = color;
}

//-----------------------------------------------------------------------------
QColor Structure::get_color()
{
  return this->color_;
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> Structure::get_mesh_union()
{
  if ( this->mesh_ )
  {
    return this->mesh_;
  }

  PointSampler ps( this );
  std::list<Weighted_point> points = ps.collect_spheres();

  std::cerr << "Generate union of spheres\n";
  Union_of_balls_3 union_of_balls( points.begin(), points.end() );
  Polyhedron P;
  CGAL::mesh_union_of_balls_3( union_of_balls, P );
  //CGAL::subdivide_union_of_balls_mesh_3(union_of_balls, P);

  double shrinkfactor = 0.5;
  //CGAL::make_skin_surface_mesh_3(P, points.begin(), points.end(), shrinkfactor);

/*
   typedef CGAL::Skin_surface_3<Traits> Skin_surface_3;
   Skin_surface_3 skin_surface(points.begin(), points.end(), shrinkfactor);
   CGAL::mesh_skin_surface_3(skin_surface, P);
   CGAL::subdivide_skin_surface_mesh_3(skin_surface, P);
 */

  std::ofstream out( "Z:\\shared\\balls.off" );

  out << P;
  out.close();

  std::cerr << "Convert resulting mesh\n";

  vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();
  vtkSmartPointer<vtkPoints> vtk_pts = vtkSmartPointer<vtkPoints>::New();
  vtkSmartPointer<vtkCellArray> vtk_triangles = vtkSmartPointer<vtkCellArray>::New();

  typedef Polyhedron::Vertex_const_iterator VCI;
  typedef Polyhedron::Facet_const_iterator FCI;
  typedef Polyhedron::Halfedge_around_facet_const_circulator HFCC;
  typedef CGAL::Inverse_index<VCI> Index;

  int vcount = 0;
  for ( VCI vi = P.vertices_begin(); vi != P.vertices_end(); ++vi )
  {
    vtk_pts->InsertNextPoint( vi->point().x(), vi->point().y(), vi->point().z() );
    vcount++;
  }

  std::cerr << "inserted " << vcount << " vertices\n";

  Index index( P.vertices_begin(), P.vertices_end() );

  int fcount = 0;
  for ( FCI fi = P.facets_begin(); fi != P.facets_end(); ++fi )
  {
    fcount++;
    HFCC hc = fi->facet_begin();
    HFCC hc_end = hc;
    std::size_t n = circulator_size( hc );
    CGAL_assertion( n >= 3 );
    if ( n != 3 )
    {
      std::cerr << "Not triangular!\n";
    }
    vtkSmartPointer<vtkTriangle> triangle = vtkSmartPointer<vtkTriangle>::New();
    int c = 0;
    do
    {
      triangle->GetPointIds()->InsertId( c++, index[ VCI( hc->vertex() )] );
      ++hc;
    }
    while ( hc != hc_end );

    vtk_triangles->InsertNextCell( triangle );
  }

  std::cerr << "inserted " << fcount << " facets\n";

  poly_data->SetPoints( vtk_pts );
  poly_data->SetPolys( vtk_triangles );

  poly_data->BuildLinks();

  vtkSmartPointer<vtkSmoothPolyDataFilter> smooth;

  vtkSmartPointer<vtkFeatureEdges> features = vtkSmartPointer<vtkFeatureEdges>::New();
  features->SetInputData( poly_data );
  features->NonManifoldEdgesOn();
  features->BoundaryEdgesOff();
  features->FeatureEdgesOff();
  features->Update();

  vtkSmartPointer<vtkPolyData> nonmanifold = features->GetOutput();

  std::cerr << "Number of non-manifold points: " << nonmanifold->GetNumberOfPoints() << "\n";
  std::cerr << "Number of non-manifold cells: " << nonmanifold->GetNumberOfCells() << "\n";

  std::cerr << "QuadricDecimation\n";

  vtkSmartPointer<vtkSTLWriter> writer = vtkSmartPointer<vtkSTLWriter>::New();
  writer->SetFileName( "Z:\\shared\\before_decimate.stl" );
  writer->SetInputData( poly_data );
  writer->Write();

  vtkSmartPointer<customQuadricDecimation> decimate = vtkSmartPointer<customQuadricDecimation>::New();
  decimate->SetInputData( poly_data );
  //decimate->SetTargetReduction(.99); //99% reduction (if there was 100 triangles, now there will be 1)
  decimate->SetTargetReduction( .98 );   //10% reduction (if there was 100 triangles, now there will be 90)
  decimate->Update();
  poly_data = decimate->GetOutput();

  writer = vtkSmartPointer<vtkSTLWriter>::New();
  writer->SetFileName( "Z:\\shared\\after_decimate.stl" );
  writer->SetInputData( poly_data );
  writer->Write();

  poly_data->BuildLinks();

  std::cerr << "after decimation, " << poly_data->GetNumberOfPoints() << " points, " << poly_data->GetNumberOfCells() << " triangles\n";

  vtkSmartPointer<vtkCleanPolyData> clean = vtkSmartPointer<vtkCleanPolyData>::New();
  clean->SetInputData( poly_data );
  clean->SetTolerance( 0.00001 );
  clean->Update();
  poly_data = clean->GetOutput();

  std::cerr << "after clean, " << poly_data->GetNumberOfPoints() << " points, " << poly_data->GetNumberOfCells() << " triangles\n";

  features = vtkSmartPointer<vtkFeatureEdges>::New();
  features->SetInputData( poly_data );
  features->NonManifoldEdgesOn();
  features->BoundaryEdgesOff();
  features->FeatureEdgesOff();
  features->Update();

  nonmanifold = features->GetOutput();

  std::cerr << "Number of non-manifold points: " << nonmanifold->GetNumberOfPoints() << "\n";
  std::cerr << "Number of non-manifold cells: " << nonmanifold->GetNumberOfCells() << "\n";

  std::cerr << "=====================Mesh Fixing====================\n";

  std::vector<int> remove;

  for ( int j = 0; j < poly_data->GetNumberOfPoints(); j++ )
  {
    double p2[3];
    poly_data->GetPoint( j, p2 );

    for ( int i = 0; i < nonmanifold->GetNumberOfPoints(); i++ )
    {
      double p[3];
      nonmanifold->GetPoint( i, p );

      if ( p[0] == p2[0] && p[1] == p2[1] && p[2] == p2[2] )
      {
        remove.push_back( j );
      }
    }
  }

  std::cerr << "Removing " << remove.size() << " non-manifold vertices\n";

  int remove_cell_count = 0;

  for ( int i = 0; i < poly_data->GetNumberOfCells(); i++ )
  {
    vtkSmartPointer<vtkIdList> list = vtkIdList::New();
    poly_data->GetCellPoints( i, list );

    for ( int j = 0; j < list->GetNumberOfIds(); j++ )
    {
      int id = list->GetId( j );
      if ( id < 0 )
      {
        std::cerr << "detected crap-up: " << id << "\n";
      }
    }
  }

/*


   for ( int i = 0; i < poly_data->GetNumberOfCells(); i++ )
   {
     vtkSmartPointer<vtkIdList> list = vtkIdList::New();
     poly_data->GetCellPoints( i, list );

     bool match = false;
     for ( int j = 0; j < list->GetNumberOfIds(); j++ )
     {
       int id = list->GetId( j );
       for ( unsigned int k = 0; k < remove.size(); k++ )
       {
         if ( id == remove[k] )
         {
           match = true;
         }
       }
     }

     if ( match )
     {
       remove_cell_count++;
       poly_data->DeleteCell( i );
     }
   }
 */

  poly_data->BuildLinks();
  for ( unsigned int k = 0; k < remove.size(); k++ )
  {
    unsigned short ncell;
    vtkIdType* cells;
    poly_data->GetPointCells( remove[k], ncell, cells );

    for ( unsigned short c = 0; c < ncell; c++ )
    {
      poly_data->DeleteCell( cells[c] );
      remove_cell_count++;
    }
  }

  poly_data->RemoveDeletedCells();

  std::cerr << "removed " << remove_cell_count << " cells\n";
  poly_data->BuildLinks();

  features = vtkSmartPointer<vtkFeatureEdges>::New();
  features->SetInputData( poly_data );
  features->NonManifoldEdgesOn();
  features->BoundaryEdgesOff();
  features->FeatureEdgesOff();
  features->Update();
  nonmanifold = features->GetOutput();
  std::cerr << "Number of non-manifold points: " << nonmanifold->GetNumberOfPoints() << "\n";
  std::cerr << "Number of non-manifold cells: " << nonmanifold->GetNumberOfCells() << "\n";

  std::cerr << "after fixing, " << poly_data->GetNumberOfPoints() << " points, " << poly_data->GetNumberOfCells() << " triangles\n";

  // fill holes
  vtkSmartPointer<vtkFillHolesFilter> fill_holes = vtkSmartPointer<vtkFillHolesFilter>::New();
  fill_holes->SetInputData( poly_data );
  fill_holes->SetHoleSize( 300 );
  fill_holes->Update();
  poly_data = fill_holes->GetOutput();

  std::cerr << "=====================Mesh Fixing====================\n";

  std::cerr << "after hole filling, " << poly_data->GetNumberOfPoints() << " points, " << poly_data->GetNumberOfCells() << " triangles\n";

  //clean = vtkSmartPointer<vtkCleanPolyData>::New();
  //clean->SetInputData( poly_data );
  //clean->SetTolerance( 0.00001 );
  //clean->Update();
  //poly_data = clean->GetOutput();

  //poly_data->BuildLinks();

  //std::cerr << "after cleaning, " << poly_data->GetNumberOfPoints() << " points, " << poly_data->GetNumberOfCells() << " triangles\n";

  features = vtkSmartPointer<vtkFeatureEdges>::New();
  features->SetInputData( poly_data );
  features->NonManifoldEdgesOn();
  features->BoundaryEdgesOff();
  features->FeatureEdgesOff();
  features->Update();
  nonmanifold = features->GetOutput();
  //std::cerr << "cleaning...\n";
  std::cerr << "Number of non-manifold points: " << nonmanifold->GetNumberOfPoints() << "\n";
  std::cerr << "Number of non-manifold cells: " << nonmanifold->GetNumberOfCells() << "\n";

/*
   std::cerr << "DecimatePro\n";
   vtkSmartPointer<vtkDecimatePro> decimate =
    vtkSmartPointer<vtkDecimatePro>::New();
   decimate->SetInputData(poly_data);
   //decimate->SetTargetReduction(.99); //99% reduction (if there was 100 triangles, now there will be 1)
   decimate->SetTargetReduction(.95); //10% reduction (if there was 100 triangles, now there will be 90)
   decimate->Update();
   poly_data = decimate->GetOutput();
 */

  vtkSmartPointer< vtkTriangleFilter > triangle_filter = vtkSmartPointer< vtkTriangleFilter >::New();
  triangle_filter->SetInputData( poly_data );
  triangle_filter->PassLinesOff();
  triangle_filter->Update();
  poly_data = triangle_filter->GetOutput();

//  poly_data->BuildLinks();

  //poly_data = this->recopy_mesh(poly_data);

  for ( int i = 0; i < poly_data->GetNumberOfCells(); i++ )
  {
    vtkSmartPointer<vtkIdList> list = vtkIdList::New();
    poly_data->GetCellPoints( i, list );

    if ( list->GetNumberOfIds() != 3 )
    {
      std::cerr << "detected non-triangle, wtf?\n";
    }
    for ( int j = 0; j < list->GetNumberOfIds(); j++ )
    {
      int id = list->GetId( j );
      if ( id < 0 )
      {
        std::cerr << "detected crap-up: " << id << "\n";
      }
    }
  }

  //vtkSmartPointer<vtkPolyData> new_poly = this->recopy_mesh(poly_data);
  //  poly_data = this->recopy_mesh(poly_data);

  //poly_data->BuildLinks();

  std::cerr << "loop subdivision\n";
  vtkSmartPointer<vtkLoopSubdivisionFilter> subdivision = vtkSmartPointer<vtkLoopSubdivisionFilter>::New();
  subdivision->SetInputData( poly_data );
  subdivision->SetNumberOfSubdivisions( 2 );
  subdivision->Update();
  poly_data = subdivision->GetOutput();

  std::cerr << "done with subdivision\n";

/*
   std::cerr << "Sinc\n";
   vtkSmartPointer<vtkWindowedSincPolyDataFilter> sinc = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
   sinc->SetInputData( poly_data );
   sinc->SetNumberOfIterations(15);
   sinc->BoundarySmoothingOn();
   sinc->FeatureEdgeSmoothingOff();
   sinc->SetFeatureAngle(120.0);
   sinc->SetPassBand(.1);
   sinc->NormalizeCoordinatesOn();
   sinc->Update();
   poly_data = sinc->GetOutput();
 */

/*
   smooth->SetInputData( poly_data );
   smooth->SetNumberOfIterations( 100 );
   smooth->FeatureEdgeSmoothingOff();
   smooth->BoundarySmoothingOn();
   smooth->Update();
   poly_data = smooth->GetOutput();
 */

/*
   vtkSmartPointer<vtkCleanPolyData> clean = vtkSmartPointer<vtkCleanPolyData>::New();
   clean->SetInputData( poly_data );
   clean->SetTolerance( 0.00001 );
   clean->Update();
   poly_data = clean->GetOutput();


   vtkSmartPointer< vtkTriangleFilter > triangle_filter =
   vtkSmartPointer< vtkTriangleFilter >::New();
   triangle_filter->SetInputData( poly_data );
   triangle_filter->Update();
   poly_data = triangle_filter->GetOutput();
 */

/*
   std::cerr << "Linear subdivision\n";
   vtkSmartPointer<vtkLinearSubdivisionFilter> subdivision = vtkSmartPointer<vtkLinearSubdivisionFilter>::New();
   subdivision->SetInputData( poly_data );
   subdivision->SetNumberOfSubdivisions( 2 );
   subdivision->Update();
   poly_data = subdivision->GetOutput();
 */

/*
   std::cerr << "butterfly subdivision\n";
   vtkSmartPointer<vtkButterflySubdivisionFilter> subdivision = vtkSmartPointer<vtkButterflySubdivisionFilter>::New();
   subdivision->SetInputData( poly_data );
   subdivision->SetNumberOfSubdivisions( 2 );
   subdivision->Update();
   poly_data = subdivision->GetOutput();
 */

/*
   std::cerr << "Butterfly\n";
   vtkSmartPointer<vtkButterflySubdivisionFilter> butterfly = vtkSmartPointer<vtkButterflySubdivisionFilter>::New();
   butterfly->SetInputData(poly_data);
   butterfly->SetNumberOfSubdivisions(1);
   butterfly->Update();
   poly_data = butterfly->GetOutput();
 */

  /*
     smooth = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
     smooth->SetInputData( poly_data );
     smooth->SetNumberOfIterations( 100 );
     smooth->FeatureEdgeSmoothingOff();
     smooth->BoundarySmoothingOn();
     smooth->Update();
     poly_data = smooth->GetOutput();
   */

/*
   std::cerr << "Sinc\n";
   sinc = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
   sinc->SetInputData( poly_data );
   sinc->SetNumberOfIterations(15);
   sinc->BoundarySmoothingOn();
   sinc->FeatureEdgeSmoothingOff();
   sinc->SetFeatureAngle(120.0);
   sinc->SetPassBand(.5);
   sinc->NormalizeCoordinatesOn();
   sinc->Update();
   poly_data = sinc->GetOutput();
 */

/*
   std::cerr << "Normals\n";
   // Make the triangle winding order consistent
   vtkSmartPointer<vtkPolyDataNormals> normals =
    vtkSmartPointer<vtkPolyDataNormals>::New();
   normals->SetInputData( poly_data );
   normals->ConsistencyOn();
   normals->SplittingOff();
   normals->Update();
   poly_data = normals->GetOutput();
 */

  writer = vtkSmartPointer<vtkSTLWriter>::New();
  writer->SetFileName( "Z:\\shared\\file.stl" );
  writer->SetInputData( poly_data );
  writer->Write();

  this->mesh_ = poly_data;
  return this->mesh_;
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> Structure::recopy_mesh( vtkSmartPointer<vtkPolyData> mesh )
{

  vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();
  vtkSmartPointer<vtkPoints> vtk_pts = vtkSmartPointer<vtkPoints>::New();
  vtkSmartPointer<vtkCellArray> vtk_triangles = vtkSmartPointer<vtkCellArray>::New();

  for ( int i = 0; i < mesh->GetNumberOfPoints(); i++ )
  {
    vtk_pts->InsertNextPoint( mesh->GetPoint( i ) );
  }

  for ( int i = 0; i < mesh->GetNumberOfCells(); i++ )
  {
    if ( mesh->GetCell( i )->GetNumberOfPoints() == 3 )
    {
      vtk_triangles->InsertNextCell( mesh->GetCell( i ) );
    }
  }

  poly_data->SetPoints( vtk_pts );
  poly_data->SetPolys( vtk_triangles );

  return poly_data;
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> Structure::get_mesh_parts()
{
  if ( this->mesh_ )
  {
    return this->mesh_;
  }

  std::cerr << "creating mesh...\n";

  NodeMap node_map = this->get_node_map();

  std::list<Point> points;

  vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();

  bool first = true;

  // spheres
  for ( NodeMap::iterator it = node_map.begin(); it != node_map.end(); ++it )
  {

    Node n = it->second;

    if ( n.linked_nodes.size() != 1 )
    {
      continue;
    }

//    std::cerr << "adding sphere: " << n.id << "(" << n.x << "," << n.y << "," << n.z << "," << n.radius << ")\n";

    vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetCenter( n.x, n.y, n.z );
    sphere->SetRadius( n.radius );
    sphere->Update();

    if ( first )
    {
      poly_data = sphere->GetOutput();
      first = false;
    }
    else
    {
/*
      vtkSmartPointer<vtkBooleanOperationPolyDataFilter> booleanOperation =
        vtkSmartPointer<vtkBooleanOperationPolyDataFilter>::New();
      booleanOperation->SetOperationToUnion();

      booleanOperation->SetInputData( 0, poly_data );
      booleanOperation->SetInputData( 1, sphere->GetOutput() );
      booleanOperation->Update();
      poly_data = booleanOperation->GetOutput();
 */

      vtkSmartPointer<vtkAppendPolyData> append = vtkSmartPointer<vtkAppendPolyData>::New();
      append->AddInputData( poly_data );
      append->AddInputData( sphere->GetOutput() );
      append->Update();
      poly_data = append->GetOutput();
    }
  }

  foreach( Link link, this->get_links() ) {

    if ( node_map.find( link.a ) == node_map.end() || node_map.find( link.b ) == node_map.end() )
    {
      continue;
    }

    Node n1 = node_map[link.a];
    Node n2 = node_map[link.b];

    vtkSmartPointer<vtkPoints> vtk_points = vtkSmartPointer<vtkPoints>::New();

    vtk_points->InsertNextPoint( n1.x, n1.y, n1.z );
    vtk_points->InsertNextPoint( n2.x, n2.y, n2.z );

    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
    lines->InsertNextCell( 2 );
    lines->InsertCellPoint( 0 );
    lines->InsertCellPoint( 1 );

    vtkSmartPointer<vtkDoubleArray> tube_radius = vtkSmartPointer<vtkDoubleArray>::New();
    tube_radius->SetName( "tube_radius" );
    tube_radius->SetNumberOfTuples( 2 );
    tube_radius->SetTuple1( 0, n1.radius );
    tube_radius->SetTuple1( 1, n2.radius );

    vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();
    poly_data->SetPoints( vtk_points );
    poly_data->SetLines( lines );
    poly_data->GetPointData()->AddArray( tube_radius );
    poly_data->GetPointData()->SetActiveScalars( "tube_radius" );

    vtkSmartPointer<vtkTubeFilter> tube = vtkSmartPointer<vtkTubeFilter>::New();
    tube->SetInputData( poly_data );
    tube->CappingOn();
    tube->SetVaryRadiusToVaryRadiusByAbsoluteScalar();
    tube->SetRadius( n1.radius );
    tube->SetNumberOfSides( 20 );
    tube->Update();

    vtkSmartPointer<vtkAppendPolyData> append = vtkSmartPointer<vtkAppendPolyData>::New();
    append->AddInputData( poly_data );
    append->AddInputData( tube->GetOutput() );
    append->Update();
    poly_data = append->GetOutput();
  }

  this->mesh_ = poly_data;

  return this->mesh_;
}

//-----------------------------------------------------------------------------
double Structure::distance( const Node &n1, const Node &n2 )
{
  double squared_dist = ( n1.x - n2.x ) * ( n1.x - n2.x )
                        + ( n1.y - n2.y ) * ( n1.y - n2.y )
                        + ( n1.z - n2.z ) * ( n1.z - n2.z );
  return sqrt( squared_dist );
}

//-----------------------------------------------------------------------------
void Structure::connect_subgraphs()
{
  long max_count = 0;

  // initialize
  for ( NodeMap::iterator it = this->node_map_.begin(); it != this->node_map_.end(); ++it )
  {
    ( it->second ).graph_id = -1;
  }

  for ( NodeMap::iterator it = this->node_map_.begin(); it != this->node_map_.end(); ++it )
  {
    Node n = it->second;

    if ( n.graph_id == -1 )
    {
      max_count++;
      n.graph_id = max_count;
      this->node_map_[it->first] = n;

      QList<int> connections = n.linked_nodes;

      while ( connections.size() > 0 )
      {
        int node = connections.first();
        connections.pop_front();

        Node child = this->node_map_[node];

        if ( child.graph_id == -1 )
        {
          child.graph_id = max_count;
          connections.append( child.linked_nodes );
          this->node_map_[node] = child;  // write back
        }
      }
    }
  }

  std::cerr << "Found " << max_count << " graphs\n";

  // create links between graphs

  QList<int> primary_group;

  for ( NodeMap::iterator it = this->node_map_.begin(); it != this->node_map_.end(); ++it )
  {
    Node n = it->second;
    if ( n.graph_id == 1 )
    {
      primary_group.append( n.id );
    }
  }

  for ( int i = 2; i <= max_count; i++ )
  {

    // find closest pair
    double min_dist = DBL_MAX;
    int primary_id = -1;
    int child_id = -1;

    for ( NodeMap::iterator it = this->node_map_.begin(); it != this->node_map_.end(); ++it )
    {
      Node n = it->second;

      if ( n.graph_id == i )
      {

        for ( NodeMap::iterator it2 = this->node_map_.begin(); it2 != this->node_map_.end(); ++it2 )
        {
          Node pn = it2->second;
          if ( pn.graph_id >= i )
          {
            continue;
          }

          double point1[3], point2[3];
          point1[0] = n.x;
          point1[1] = n.y;
          point1[2] = n.z;
          point2[0] = pn.x;
          point2[1] = pn.y;
          point2[2] = pn.z;
          double distance = sqrt( vtkMath::Distance2BetweenPoints( point1, point2 ) );

          if ( distance < min_dist )
          {
            min_dist = distance;
            primary_id = pn.id;
            child_id = n.id;
          }
        }
      }
    }

    Link new_link;
    new_link.a = primary_id;
    new_link.b = child_id;
    this->links_.append( new_link );

    this->node_map_[primary_id].linked_nodes.append( child_id );
    this->node_map_[child_id].linked_nodes.append( primary_id );
  }
}

//-----------------------------------------------------------------------------
void Structure::cull_locations()
{

  std::vector<int> remove_list;
  do
  {
    remove_list.clear();
    // cull overlapping locations
    for ( NodeMap::iterator it = this->node_map_.begin(); it != this->node_map_.end(); ++it )
    {
      Node n = it->second;

      if ( n.linked_nodes.size() != 2 )
      {
        //continue;
      }

      bool removed = false;

      int other_id = -1;

      foreach( int id, n.linked_nodes ) {

        if ( !removed )
        {
          Node other = this->node_map_[id];

          if (other.linked_nodes.size() <= n.linked_nodes.size())  // remove the one with less links
          {
            if ( distance( n, other ) < std::max( n.radius, other.radius ) )
            {
              remove_list.push_back( n.id );
              other_id = other.id;
            removed = true;
            }
          }
        }
      }

      if ( removed )
      {
        this->node_map_[other_id].linked_nodes.remove( n.id );

        foreach( int id, n.linked_nodes ) {

          if (id != other_id)
          {
            this->node_map_[other_id].linked_nodes.append( id );


            this->node_map_[id].linked_nodes.remove( n.id );
            this->node_map_[id].linked_nodes.append( other_id );
          }
        }

/*

        int n1 = n.linked_nodes[0];
        int n2 = n.linked_nodes[1];

        this->node_map_[n1].linked_nodes.remove( n.id );
        this->node_map_[n1].linked_nodes.append( n2 );
        this->node_map_[n2].linked_nodes.remove( n.id );
        this->node_map_[n2].linked_nodes.append( n1 );*/

      }
    }

    std::cerr << "remove list size : " << remove_list.size() << "\n";

    for ( unsigned int i = 0; i < remove_list.size(); i++ )
    {
      this->node_map_.erase( remove_list[i] );
    }

    this->connect_subgraphs();
  }
  while ( remove_list.size() > 0 );
}

//-----------------------------------------------------------------------------
void Structure::link_report()
{
  std::vector<int> link_counts( 100 );

  for ( NodeMap::iterator it = this->node_map_.begin(); it != this->node_map_.end(); ++it )
  {
    Node n = it->second;
    link_counts[n.linked_nodes.size()]++;
  }

  for ( int i = 0; i < 100; i++ )
  {
    if ( link_counts[i] > 0 )
    {
      std::cerr << "Nodes with " << i << " links: " << link_counts[i] << "\n";
    }
  }
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> Structure::get_mesh_tubes()
{
  if ( this->mesh_ )
  {
    return this->mesh_;
  }

  std::cerr << "creating mesh...\n";

  //NodeMap node_map = this->get_node_map();

  std::list<Point> points;

  vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();

  // reset visited
  for ( NodeMap::iterator it = this->node_map_.begin(); it != this->node_map_.end(); ++it )
  {
    ( it->second ).visited = false;
  }

  int root = -1;
  // find a dead-end
  for ( NodeMap::iterator it = this->node_map_.begin(); it != this->node_map_.end(); ++it )
  {
    Node n = it->second;
    if ( n.linked_nodes.size() == 1 )
    {
      root = n.id;
      break;
    }
  }

  if ( root == -1 )
  {
    std::cerr << "Error: could not locate root node\n";
    return this->mesh_;
  }

  Node n = this->node_map_[root];

  vtkSmartPointer<vtkAppendPolyData> append = vtkSmartPointer<vtkAppendPolyData>::New();

  this->add_polydata( n, -1, append, QList<int>() );

  std::cerr << "Num of items: " << append->GetNumberOfInputConnections( 0 ) << "\n";

  std::cerr << "number of tubes: " << this->num_tubes_ << "\n";

  append->Update();
  poly_data = append->GetOutput();


  std::cerr << "number of verts: " << poly_data->GetNumberOfPoints() << "\n";
  std::cerr << "number of polys: " << poly_data->GetNumberOfCells() << "\n";


  this->mesh_ = poly_data;

  return this->mesh_;
}

//-----------------------------------------------------------------------------
void Structure::add_polydata( Node n, int from, vtkSmartPointer<vtkAppendPolyData> append, QList<int> current_line )
{
  this->node_map_[n.id].visited = true;

  if ( n.linked_nodes.size() == 2 )
  {
    current_line.append( n.id );

    for ( int i = 0; i < n.linked_nodes.size(); i++ )
    {
      Node other = this->node_map_[n.linked_nodes[i]];
      if ( !other.visited )
      {
        this->add_polydata( other, n.id, append, current_line );
      }
    }
  }

  if ( n.linked_nodes.size() != 2 )
  {
    vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetCenter( n.x, n.y, n.z );
    sphere->SetRadius( n.radius * 1.1 );

    int resolution = 10;
    if (n.radius > 1.0)
    {
      resolution = resolution * n.radius;
    }

    resolution = 12;

    sphere->SetPhiResolution(resolution);
    sphere->SetThetaResolution(resolution);
    sphere->Update();

    vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();
    poly_data = sphere->GetOutput();

    vtkSmartPointer<vtkUnsignedCharArray> colors =
      vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents( 3 );
    colors->SetName( "Colors" );

    int r = 128 + ( qrand() % 128 );
    int g = 128 + ( qrand() % 128 );
    int b = 128 + ( qrand() % 128 );

    for ( int i = 0; i < poly_data->GetNumberOfPoints(); ++i )
    {
      unsigned char tempColor[3] =
      {r, g, b};

      colors->InsertNextTupleValue( tempColor );
    }


    //poly_data->GetPointData()->SetScalars( colors );


/*
    vtkSmartPointer< vtkTriangleFilter > triangle_filter = vtkSmartPointer< vtkTriangleFilter >::New();
    triangle_filter->SetInputData( poly_data );
    //    triangle_filter->PassLinesOff();
    triangle_filter->Update();
    poly_data = triangle_filter->GetOutput();

    std::cerr << "Number of points before cleaning: " << poly_data->GetNumberOfPoints() << "\n";
    vtkSmartPointer<vtkCleanPolyData> clean = vtkSmartPointer<vtkCleanPolyData>::New();
    clean->SetInputData( poly_data );
    //clean->SetTolerance( 0.00001 );
    clean->Update();
    poly_data = clean->GetOutput();
    std::cerr << "Number of points after cleaning: " << poly_data->GetNumberOfPoints() << "\n";

    this->num_tubes_++;
    //QString filename = QString("C:\\Users\\amorris\\part") + QString::number(this->num_tubes_) + ".ply";
    QString filename = QString("C:\\Users\\amorris\\part") + QString::number(this->num_tubes_) + ".vtk";
    vtkSmartPointer<vtkPolyDataWriter> writer4 = vtkSmartPointer<vtkPolyDataWriter>::New();
    //vtkSmartPointer<vtkPLYWriter> writer4 = vtkSmartPointer<vtkPLYWriter>::New();
    writer4->SetFileName( filename );
    writer4->SetInputData( poly_data );
    //writer4->SetFileTypeToBinary();
    writer4->Write();
*/



    append->AddInputData( poly_data );

    if ( current_line.size() > 0 )
    {
      current_line.append( n.id );

      vtkSmartPointer<vtkPoints> vtk_points = vtkSmartPointer<vtkPoints>::New();
      vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
      lines->InsertNextCell( current_line.size() );
      vtkSmartPointer<vtkDoubleArray> tube_radius = vtkSmartPointer<vtkDoubleArray>::New();
      tube_radius->SetName( "tube_radius" );

      vtkSmartPointer<vtkTupleInterpolator> interpolatedRadius =
        vtkSmartPointer<vtkTupleInterpolator> ::New();
      interpolatedRadius->SetInterpolationTypeToLinear();
      interpolatedRadius->SetNumberOfComponents( 1 );

      int count = 0;
      foreach( int node_id, current_line ) {
        Node node = this->node_map_[node_id];

        vtk_points->InsertNextPoint( node.x, node.y, node.z );
        lines->InsertCellPoint( count++ );
        tube_radius->InsertNextTuple1( node.radius );
        interpolatedRadius->AddTuple( count, &( node.radius ) );
      }

      vtkSmartPointer<vtkParametricSpline> spline =
        vtkSmartPointer<vtkParametricSpline>::New();
      spline->SetPoints( vtk_points );

      // Interpolate the points
      vtkSmartPointer<vtkParametricFunctionSource> functionSource =
        vtkSmartPointer<vtkParametricFunctionSource>::New();
      functionSource->SetParametricFunction( spline );
      functionSource->SetUResolution( 2 * vtk_points->GetNumberOfPoints() );
      //functionSource->SetUResolution( 5 );
      functionSource->Update();

      vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();
      poly_data->SetPoints( vtk_points );
      poly_data->SetLines( lines );
      poly_data->GetPointData()->AddArray( tube_radius );
      poly_data->GetPointData()->SetActiveScalars( "tube_radius" );

      // Generate the radius scalars
      vtkSmartPointer<vtkDoubleArray> tubeRadius = vtkSmartPointer<vtkDoubleArray>::New();
      unsigned int n = functionSource->GetOutput()->GetNumberOfPoints();
      tubeRadius->SetNumberOfTuples( n );
      tubeRadius->SetName( "TubeRadius" );
      double tMin = interpolatedRadius->GetMinimumT();
      double tMax = interpolatedRadius->GetMaximumT();
      double radius;
      for ( unsigned int i = 0; i < n; ++i )
      {
        double t = ( tMax - tMin ) / ( n - 1 ) * i + tMin;
        interpolatedRadius->InterpolateTuple( t, &radius );
        tubeRadius->SetTuple1( i, radius );
      }

      // Add the scalars to the polydata
      vtkSmartPointer<vtkPolyData> tubePolyData =
        vtkSmartPointer<vtkPolyData>::New();
      tubePolyData = functionSource->GetOutput();
      tubePolyData->GetPointData()->AddArray( tubeRadius );
      tubePolyData->GetPointData()->SetActiveScalars( "TubeRadius" );

      vtkSmartPointer<vtkTubeFilter> tube = vtkSmartPointer<vtkTubeFilter>::New();
      //tube->SetInputData( poly_data );

      //tube->SetInputData( functionSource->GetOutput() );
      tube->SetInputData( tubePolyData );

      tube->CappingOn();
      tube->SetVaryRadiusToVaryRadiusByAbsoluteScalar();

      //tube->SetRadius(n.radius);
      //tube->SetRadius( 0.1 );
      tube->SetNumberOfSides( 15 );
      tube->Update();

      poly_data = tube->GetOutput();

      vtkSmartPointer<vtkUnsignedCharArray> colors =
        vtkSmartPointer<vtkUnsignedCharArray>::New();
      colors->SetNumberOfComponents( 3 );
      colors->SetName( "Colors" );

      int r = 128 + ( qrand() % 128 );
      int g = 128 + ( qrand() % 128 );
      int b = 128 + ( qrand() % 128 );

      for ( int i = 0; i < poly_data->GetNumberOfPoints(); ++i )
      {
        unsigned char tempColor[3] =
        {r, g, b};

        colors->InsertNextTupleValue( tempColor );
      }

      //poly_data->GetPointData()->SetScalars( colors );

      append->AddInputData( poly_data );
/*


      vtkSmartPointer< vtkTriangleFilter > triangle_filter = vtkSmartPointer< vtkTriangleFilter >::New();
      triangle_filter->SetInputData( poly_data );
      //    triangle_filter->PassLinesOff();
      triangle_filter->Update();
      poly_data = triangle_filter->GetOutput();

      std::cerr << "Number of points before cleaning: " << poly_data->GetNumberOfPoints() << "\n";
      vtkSmartPointer<vtkCleanPolyData> clean = vtkSmartPointer<vtkCleanPolyData>::New();
      clean->SetInputData( poly_data );
      //clean->SetTolerance( 0.00001 );
      clean->Update();
      poly_data = clean->GetOutput();
      std::cerr << "Number of points after cleaning: " << poly_data->GetNumberOfPoints() << "\n";



      this->num_tubes_++;
      //QString filename = QString("C:\\Users\\amorris\\part") + QString::number(this->num_tubes_) + ".ply";
      QString filename = QString("C:\\Users\\amorris\\part") + QString::number(this->num_tubes_) + ".vtk";
      //vtkSmartPointer<vtkPLYWriter> writer4 = vtkSmartPointer<vtkPLYWriter>::New();
      vtkSmartPointer<vtkPolyDataWriter> writer4 = vtkSmartPointer<vtkPolyDataWriter>::New();
      writer4->SetFileName( filename );
      writer4->SetInputData( poly_data );
      //writer4->SetFileTypeToBinary();
      writer4->Write();


      filename = QString("C:\\Users\\amorris\\part") + QString::number(this->num_tubes_) + ".stl";
      vtkSmartPointer<vtkSTLWriter> writer = vtkSmartPointer<vtkSTLWriter>::New();
      writer->SetFileName( filename );
      writer->SetInputData( poly_data );
      writer->Write();

*/
    }

    for ( int i = 0; i < n.linked_nodes.size(); i++ )
    {
      Node other = this->node_map_[n.linked_nodes[i]];
      if ( !other.visited )
      {
        QList<int> new_line;
        new_line.append( n.id );
        this->add_polydata( other, n.id, append, new_line );
      }
    }
  }
}
