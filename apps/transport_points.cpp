// This file is part of otmap, an optimal transport solver.
//
// Copyright (C) 2017-2018 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2017 Georges Nader
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>
#include "otsolver_2dgrid.h"
#include "common/otsolver_options.h"
#include "utils/eigen_addons.h"
#include "common/image_utils.h"
#include "common/generic_tasks.h"
#include "utils/BenchTimer.h"
#include <surface_mesh/Surface_mesh.h>

#include "normal_integration/normal_integration.h"
#include "normal_integration/mesh.h"

using namespace Eigen;
using namespace surface_mesh;
using namespace otmap;

void output_usage()
{
  std::cout << "usage : sample <option> <value>" << std::endl;

  std::cout << std::endl;

  std::cout << "input options : " << std::endl;
  std::cout << " * -in <filename> -> input image" << std::endl;

  std::cout << std::endl;

  CLI_OTSolverOptions::print_help();

  std::cout << std::endl;

  std::cout << " * -ores <res1> <res2> <res3> ... -> ouput point resolutions" << std::endl;
  std::cout << " * -ptscale <value>               -> scaling factor to apply to SVG point sizes (default 1)" << std::endl;
  std::cout << " * -pattern <value>               -> pattern = poisson or a .dat file, default is tiling from uniform_pattern_sig2012.dat" << std::endl;
  std::cout << " * -export_maps                   -> write maps as .off files" << std::endl;

  std::cout << std::endl;

  std::cout << "output options :" << std::endl;
  std::cout << " * -out <prefix>" << std::endl;
}

struct CLIopts : CLI_OTSolverOptions
{
  std::string filename_src;
  std::string filename_trg;

  VectorXi ores;
  double pt_scale;
  std::string pattern;
  bool inv_mode;
  bool export_maps;

  uint resolution;

  std::string out_prefix;

  void set_default()
  {
    filename_src = "";
    filename_trg = "";

    ores.resize(1); ores.setZero();
    ores(0) = 1;

    out_prefix = "";

    pt_scale = 1;
    export_maps = 0;
    pattern = "";

    resolution = 100;

    CLI_OTSolverOptions::set_default();
  }

  bool load(const InputParser &args)
  {
    set_default();

    CLI_OTSolverOptions::load(args);

    std::vector<std::string> value;

    if(args.getCmdOption("-in_src", value))
      filename_src = value[0];
    else
      return false;

    if(args.getCmdOption("-in_trg", value))
      filename_trg = value[0];
    else
      return false;

    /*if(args.getCmdOption("-points", value))
      pattern = value[0];
    else
      return false;*/

    if(args.getCmdOption("-res", value))
      resolution = std::atof(value[0].c_str());

    if(args.getCmdOption("-ptscale", value))
      pt_scale = std::atof(value[0].c_str());
    
    if(args.cmdOptionExists("-export_maps"))
      export_maps = true;

    return true;
  }
};


int main(int argc, char** argv)
{
  setlocale(LC_ALL,"C");

  InputParser input(argc, argv);

  if(input.cmdOptionExists("-help") || input.cmdOptionExists("-h")){
    output_usage();
    return 0;
  }

  CLIopts opts;
  if(!opts.load(input)){
    std::cerr << "invalid input" << std::endl;
    output_usage();
    return EXIT_FAILURE;
  }

  
  Mesh mesh(1.0, 1.0, opts.resolution, opts.resolution);
  std::vector<Eigen::Vector2d> tile;
  normal_integration normal_int;
  /*if(!load_point_cloud_dat(opts.pattern, tile))
  {
    std::cerr << "Error loading tile \"" << opts.pattern << "\"\n";
    return EXIT_FAILURE;
  }*/

  for (int i=0; i<mesh.source_points.size(); i++)
  {
    Eigen::Vector2d point = {mesh.source_points[i][0], mesh.source_points[i][1]};
    tile.push_back(point);
  }

  normal_int.initialize_data(mesh);

  GridBasedTransportSolver otsolver;
  otsolver.set_verbose_level(opts.verbose_level-1);

  if(opts.verbose_level>=1)
    std::cout << "Generate transport map...\n";

  MatrixXd density_src;
  MatrixXd density_trg;
  if(!load_input_density(opts.filename_src, density_src))
  {
    std::cout << "Failed to load input \"" << opts.filename_src << "\" -> abort.";
    exit(EXIT_FAILURE);
  }

  if(!load_input_density(opts.filename_trg, density_trg))
  {
    std::cout << "Failed to load input \"" << opts.filename_trg << "\" -> abort.";
    exit(EXIT_FAILURE);
  }

  if(density_src.maxCoeff()>1.)
    density_src = density_src / density_src.maxCoeff(); //normalize

  if(density_trg.maxCoeff()>1.)
    density_trg = density_trg / density_trg.maxCoeff(); //normalize

  //density_src = 1. - density_src.array();
  //density_trg = 1. - density_trg.array();

  //save_image((opts.out_prefix + "_target.png").c_str(), 1.-density_src.array());

  BenchTimer t_solver_init, t_solver_compute, t_generate_uniform, t_inverse;

  t_solver_init.start();
  otsolver.init(density_src.rows());
  t_solver_init.stop();

  t_solver_compute.start();
  TransportMap tmap_src = otsolver.solve(vec(density_src), opts.solver_opt);
  t_solver_compute.stop();

  t_solver_init.start();
  otsolver.init(density_trg.rows());
  t_solver_init.stop();

  t_solver_compute.start();
  TransportMap tmap_trg = otsolver.solve(vec(density_trg), opts.solver_opt);
  t_solver_compute.stop();

  std::cout << "STATS solver -- init: " << t_solver_init.value(REAL_TIMER) << "s  solve: " << t_solver_compute.value(REAL_TIMER) << "s\n";

  for(unsigned int i=0; i<opts.ores.size(); ++i){
    
    std::vector<Eigen::Vector2d> points;
    //t_generate_uniform.start();
    //generate_blue_noise_tile(opts.ores[i], points, tile);
    //t_generate_uniform.stop();

    double min_x, min_y = 1000000.0f;
    double max_x, max_y = 0.0f;

    for (int j=0; j<tile.size(); j++) {
      if (max_x < tile[j].x()) {max_x = tile[j].x();}
      if (max_y < tile[j].y()) {max_y = tile[j].y();}

      if (min_x > tile[j].x()) {min_x = tile[j].x();}
      if (min_y > tile[j].y()) {min_y = tile[j].y();}
      //tile[j].x() *= 2;
      //tile[j].y() *= 2;
    }

    for (int j=0; j<tile.size(); j++) {
      tile[j].x() -= min_x;
      tile[j].y() -= min_y;

      tile[j].x() /= (max_x - min_x);
      tile[j].y() /= (max_y - min_y);

      /*double temp = tile[j].x();
      tile[j].x() = 1-tile[j].y();
      tile[j].y() = temp;*/
    }

    printf("max_x = %f, max_y = %f\r\n", max_x, max_y);

    // compute inverse map
    //Surface_mesh inv_map = tmap_src.origin_mesh();
    //apply_inverse_map(tmap_src, inv_map.points(), opts.verbose_level);
    //inv_map.write("./test.obj");

    t_inverse.start();
    apply_forward_map(tmap_src, tile, opts.verbose_level-1);
    apply_inverse_map(tmap_trg, tile, opts.verbose_level-1);
    t_inverse.stop();

    std::vector<std::vector<double>> trg_pts;
    for (int i=0; i<mesh.source_points.size(); i++)
    {
      std::vector<double> point = {tile[i].x(), tile[i].y(), 0};
      trg_pts.push_back(point);
    }

    std::vector<std::vector<double>> desired_normals;

    for (int i=0; i<20; i++)
    {
        std::vector<std::vector<double>> normals = mesh.calculate_refractive_normals_uniform(trg_pts, 2, 1.49);

        desired_normals.clear();

        // make a copy of the original positions of the vertices
        for (int i = 0; i < mesh.source_points.size(); i++) {
            std::vector<double> trg_normal = {normals[0][i], normals[1][i], normals[2][i]};
            desired_normals.push_back(trg_normal);
        }

        normal_int.perform_normal_integration(mesh, desired_normals);
    }

    /*std::string filename = opts.out_prefix + "_" + std::to_string(opts.ores[i]);
    save_point_cloud_dat(filename + ".dat", tile);
    save_point_cloud_eps(filename + ".eps", tile, opts.pt_scale);

    std::cout << " # " << opts.ores[i] << "/" << tile.size()
                << "  ;  gen: " << t_generate_uniform.value(REAL_TIMER)
                << "s  ;  bvh+inverse: " << t_inverse.value(REAL_TIMER) << "s\n";*/

    mesh.save_solid_obj_source(0.2, "../output.obj");
  }
}
