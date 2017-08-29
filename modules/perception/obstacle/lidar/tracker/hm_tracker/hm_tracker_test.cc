/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "modules/perception/obstacle/lidar/tracker/hm_tracker/hm_tracker.h"

#include <pcl/io/pcd_io.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

#include "gtest/gtest.h"
#include "modules/common/log.h"
#include "modules/perception/common/perception_gflags.h"
#include "modules/perception/lib/config_manager/config_manager.h"
#include "modules/perception/lib/pcl_util/pcl_types.h"
#include "modules/perception/obstacle/common/file_system_util.h"
#include "modules/perception/obstacle/common/pose_util.h"
#include "modules/perception/obstacle/lidar/object_builder/min_box/min_box.h"

namespace apollo {
namespace perception {

class HmObjectTrackerTest : public testing::Test {
 protected:
  void SetUp() {
    RegisterFactoryHmObjectTracker();
    FLAGS_work_root = "modules/perception";
    FLAGS_config_manager_path = "conf/config_manager.config";
    if (!ConfigManager::instance()->Init()) {
      AERROR << "failed to init ConfigManager";
      return;
    }
    hm_tracker_ = new HmObjectTracker();
    object_builder_ = new MinBoxObjectBuilder();
    object_builder_->Init();
    object_builder_options_.ref_center =
        Eigen::Vector3d(0, 0, -1.7);  // velodyne height
    tracker_options_.velodyne_trans.reset(new Eigen::Matrix4d);
  }
  void TearDown() {
    delete hm_tracker_;
    hm_tracker_ = NULL;
    delete object_builder_;
    object_builder_ = NULL;
  }

 protected:
  HmObjectTracker* hm_tracker_;
  MinBoxObjectBuilder* object_builder_;
  ObjectBuilderOptions object_builder_options_;
  TrackerOptions tracker_options_;
};

bool ConstructObjects(const std::string& filename,
                      std::vector<ObjectPtr>* objects) {
  std::ifstream ifs(filename);
  if (!ifs.is_open()) {
    AERROR << "failed to open file" << filename;
    return false;
  }
  std::string type;
  int no_point = 0;
  float tmp = 0;
  while (ifs >> type) {
    ifs >> tmp >> tmp >> tmp >> no_point;
    ObjectPtr obj(new Object());
    obj->cloud->resize(no_point);
    for (int j = 0; j < no_point; ++j) {
      ifs >> obj->cloud->points[j].x >> obj->cloud->points[j].y >>
          obj->cloud->points[j].z >> obj->cloud->points[j].intensity;
    }
    (*objects).push_back(obj);
  }
  return true;
}

bool LoadPclPcds(const std::string filename,
                 pcl_util::PointCloudPtr* cloud_out) {
  pcl::PointCloud<pcl_util::PointXYZIT> org_cloud;
  if (pcl::io::loadPCDFile(filename, org_cloud) < 0) {
    AERROR << "failed to load pcd file: " << filename;
    return false;
  }
  (*cloud_out)->points.reserve(org_cloud.points.size());
  for (size_t i = 0; i < org_cloud.points.size(); ++i) {
    pcl_util::Point pt;
    pt.x = org_cloud.points[i].x;
    pt.y = org_cloud.points[i].y;
    pt.z = org_cloud.points[i].z;
    pt.intensity = org_cloud.points[i].intensity;
    if (isnan(org_cloud.points[i].x)) continue;
    (*cloud_out)->push_back(pt);
  }
  return true;
}

bool SaveTrackingResults(const std::vector<ObjectPtr>& objects,
                         const Eigen::Matrix4d& pose_v2w, const int frame_id,
                         pcl_util::PointCloudPtr* cloud,
                         const std::string& filename) {
  std::ofstream fout(filename.c_str(), std::ios::out);
  if (!fout) {
    AERROR << filename << " is not exist!";
    return false;
  }
  fout << frame_id << " " << objects.size() << std::endl;
  typename pcl::PointCloud<pcl_util::Point>::Ptr trans_cloud(
      new pcl::PointCloud<pcl_util::Point>());

  Eigen::Matrix4d pose_velo2w = pose_v2w;
  pcl::copyPointCloud(*(*cloud), *trans_cloud);
  TransformPointCloud<pcl_util::Point>(pose_v2w, trans_cloud);
  pcl::KdTreeFLANN<pcl_util::Point> pcl_kdtree;
  pcl_kdtree.setInputCloud(trans_cloud);
  std::vector<int> k_indices;
  std::vector<float> k_sqrt_dist;
  Eigen::Matrix4d pose_w2v = pose_velo2w.inverse();

  for (size_t i = 0; i < objects.size(); ++i) {
    Eigen::Vector3f coord_dir(0, 1, 0);
    Eigen::Vector4d dir_velo =
        pose_w2v * Eigen::Vector4d(objects[i]->direction[0],
                                   objects[i]->direction[1],
                                   objects[i]->direction[2], 0);
    Eigen::Vector4d ct_velo =
        pose_w2v * Eigen::Vector4d(objects[i]->center[0], objects[i]->center[1],
                                   objects[i]->center[2], 0);
    Eigen::Vector3f dir_velo3(dir_velo[0], dir_velo[1], dir_velo[2]);
    double theta = VectorTheta2dXy(coord_dir, dir_velo3);
    std::string type = "unknown";
    fout << objects[i]->id << " " << objects[i]->track_id << " " << type << " "
         << std::setprecision(10) << ct_velo[0] << " " << ct_velo[1] << " "
         << ct_velo[2] << " " << objects[i]->length << " " << objects[i]->width
         << " " << objects[i]->height << " " << theta << " " << 0 << " " << 0
         << " " << objects[i]->velocity[0] << " " << objects[i]->velocity[1]
         << " " << objects[i]->velocity[2] << " " << objects[i]->cloud->size()
         << " ";
    for (size_t j = 0; j < objects[i]->cloud->size(); ++j) {
      const pcl_util::Point& pt = objects[i]->cloud->points[j];
      pcl_util::Point query_pt;
      query_pt.x = pt.x;
      query_pt.y = pt.y;
      query_pt.z = pt.z;
      k_indices.resize(1);
      k_sqrt_dist.resize(1);
      pcl_kdtree.nearestKSearch(query_pt, 1, k_indices, k_sqrt_dist);
      fout << k_indices[0] << " ";
    }
    fout << std::endl;
  }
  return true;
}

TEST_F(HmObjectTrackerTest, demo_tracking) {
  // test initialization of hm tracker
  EXPECT_TRUE(hm_tracker_->Init());
  // test tracking via hm tracker
  std::string data_path = "modules/perception/data/hm_tracker_test/";
  std::vector<std::string> pcd_filenames;
  GetFileNamesInFolderById(data_path, ".pcd", &pcd_filenames);
  std::vector<std::string> seg_filenames;
  GetFileNamesInFolderById(data_path, ".seg", &seg_filenames);
  std::vector<std::string> pose_filenames;
  GetFileNamesInFolderById(data_path, ".pose", &pose_filenames);
  int frame_id = -1;
  double time_stamp = 0.0;
  EXPECT_GT(pcd_filenames.size(), 0);
  EXPECT_EQ(pcd_filenames.size(), seg_filenames.size());
  EXPECT_EQ(pcd_filenames.size(), pose_filenames.size());
  Eigen::Vector3d global_offset(0, 0, 0);
  for (size_t i = 0; i < seg_filenames.size(); ++i) {
    // load frame clouds
    pcl_util::PointCloudPtr cloud(new pcl_util::PointCloud);
    EXPECT_TRUE(LoadPclPcds(data_path + pcd_filenames[i], &cloud));
    // read pose
    Eigen::Matrix4d pose = Eigen::Matrix4d::Identity();
    if (!ReadPoseFile(data_path + pose_filenames[i], &pose, &frame_id,
                      &time_stamp)) {
      AERROR << "failed to read pose";
      return;
    }
    if (i == 0) {
      global_offset(0) = pose(0, 3);
      global_offset(1) = pose(1, 3);
      global_offset(2) = pose(2, 3);
    }
    pose(0, 3) -= global_offset(0);
    pose(1, 3) -= global_offset(1);
    pose(2, 3) -= global_offset(2);
    // read segments
    std::vector<ObjectPtr> objects;
    if (!ConstructObjects(data_path + seg_filenames[i], &objects)) {
      AERROR << "failed to read segments";
      return;
    }
    // build objects
    object_builder_->Build(object_builder_options_, &objects);
    // test tracking
    *(tracker_options_.velodyne_trans) = pose;
    std::vector<ObjectPtr> result_objects;
    // assert tracking succesfully
    EXPECT_TRUE(hm_tracker_->Track(objects, time_stamp, tracker_options_,
                                   &result_objects));
    // assert reports completly
    EXPECT_TRUE(result_objects.size() >= objects.size());
    std::map<int, int> id_pool;
    for (size_t j = 0; j < result_objects.size(); ++j) {
      int track_id = result_objects[j]->track_id;
      // assert no duplicated track id in the same frame
      EXPECT_TRUE(id_pool.find(track_id) == id_pool.end());
      id_pool[track_id] = 1;
    }
  }
}

}  // namespace perception
}  // namespace apollo
