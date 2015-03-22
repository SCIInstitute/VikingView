#ifndef VIKING_DATA_STRUCTURE_H
#define VIKING_DATA_STRUCTURE_H

#include <map>

#include <QSharedPointer.h>
#include <QColor.h>

#include <vtkSmartPointer.h>

class vtkPolyData;
class vtkAppendPolyData;

class Node
{
public:
  double x, y, z, radius;
  long id;
  QList<int> linked_nodes;
  long graph_id;
  bool visited;
};

typedef std::map<long, Node> NodeMap;

class Link
{
public:
  long a, b;
};

//! Maintains data a structure (e.g. cell)
class Structure
{

public: ~Structure();

  static QSharedPointer<Structure> create_structure( int id, QList<QVariant> structure_list,
                                                     QList<QVariant> location_list, QList<QVariant> link_list );

  NodeMap get_node_map();

  QList<Link> get_links();

  vtkSmartPointer<vtkPolyData> get_mesh_old();
  vtkSmartPointer<vtkPolyData> get_mesh_alpha();

  vtkSmartPointer<vtkPolyData> get_mesh_union();

  vtkSmartPointer<vtkPolyData> get_mesh_parts();

  vtkSmartPointer<vtkPolyData> get_mesh_tubes();

  double get_volume();

  QString get_center_of_mass_string();

  int get_id();

  void set_color( QColor color );

  QColor get_color();

  vtkSmartPointer<vtkPolyData> recopy_mesh( vtkSmartPointer<vtkPolyData> mesh );

private:

  Structure(); // private


  void add_polydata(Node n, int from, vtkSmartPointer<vtkAppendPolyData> append, QList<int> current_line);


  static double distance(const Node &n1, const Node &n2);


  void connect_subgraphs();

  void cull_locations();

  void link_report();

  int id_;
  NodeMap node_map_;

  QList<Link> links_;
  vtkSmartPointer<vtkPolyData> mesh_;

  QColor color_;

  int num_tubes_;

//  float color_[3];
};

#endif /* VIKING_DATA_STRUCTURE_H */
