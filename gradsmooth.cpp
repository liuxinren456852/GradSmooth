/*  Copyright 2016 Patrick A. O'Neil
    GradSmooth: Point cloud smoothing via distance to measure
    gradient flows.

    Author: Patrick A. O'Neil

License:
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <include/smoother.h>
#include <include/PointCloud.h>

#include <gflags/gflags.h>
#include <cnpy/cnpy.h>
#include <plog/Log.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include <string>

// Command line args
DEFINE_bool(normal_projection, false, "Project gradient onto estimated normals");
DEFINE_bool(lock_neighbors, false, "Lock neighbor calculation and use the same neighbors throughout");
DEFINE_double(step_size_normal, 0.10, "Step size for gradient flow");
DEFINE_double(step_size_tangent, 0.0, "Step size for gradient flow in tangent directions.");
DEFINE_int32(num_neighbors, 5, "Number of nearest neighbors to use for knn-search");
DEFINE_int32(iterations, 10, "Number of iterations to run the smoothing algorithm");
DEFINE_int32(max_leaf_size, 10, "Maximum number of points contained within a kd-tree leaf");
DEFINE_int32(num_threads, 1, "Number of threads to use for the smoothing algorithm");
DEFINE_int32(codimension, 1, "Co-dimension of the manifold from which the point cloud was sampled");

int main(int argc, char** argv) {
    // Set up logging
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::info, &consoleAppender);
    LOG_INFO << "Starting GradSmooth.";

    // Parse command line flags
    gflags::SetUsageMessage("GradSmooth: Arbitrary dimension point cloud smoothing.");
    unsigned arg_indx = gflags::ParseCommandLineFlags(&argc, &argv, true);

    // Get input and output path for numpy arrays
    if (argc != 3) {
        LOG_ERROR << "Incorrect number of command line args."
                   << " Please specify input and output path.";
        return 0;
    }
    std::string infn  = argv[arg_indx];
    std::string outfn = argv[arg_indx + 1];
    LOG_INFO << "Using input path: " <<  infn;
    LOG_INFO << "Using output path: " << outfn;

    PointCloud  point_cloud;
    PointCloud  evolved_cloud;

    point_cloud.load_cloud(infn);

    if (FLAGS_codimension >= point_cloud.get_dimension()) {
        LOG_FATAL << "Loaded point cloud with lower dimension than codimension!";
        return 0;
    }

    evolved_cloud.copy_cloud(point_cloud);

    LOG_INFO  << "Building k-d Tree";
    KDTree kd_tree = KDTree(point_cloud.get_dimension(), *point_cloud.get_cloud(), FLAGS_max_leaf_size);
    kd_tree.index -> buildIndex();

    LOG_DEBUG << "Successfully populated k-d Tree with " << kd_tree.kdtree_get_point_count() << " points";

    point_cloud.assign_kd_tree(&kd_tree, FLAGS_num_neighbors, FLAGS_lock_neighbors, FLAGS_num_threads);

    Smoother smoother(FLAGS_num_neighbors, point_cloud.get_dimension(), FLAGS_codimension, FLAGS_num_threads,
                      FLAGS_step_size_normal, FLAGS_step_size_tangent, FLAGS_normal_projection, FLAGS_lock_neighbors);

    smoother.smooth_point_cloud(point_cloud, evolved_cloud, FLAGS_iterations);

    evolved_cloud.save_cloud(outfn);

    return 0;
}
