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

#include "modules/perception/obstacle/lidar/segmentation/cnnseg/cnn_segmentation.h"
#include "modules/common/util/file.h"
#include "modules/perception/lib/config_manager/config_manager.h"
#include "modules/perception/lib/base/file_util.h"

using std::string;
using std::vector;

namespace apollo {
namespace perception {

bool CNNSegmentation::Init() {
  string config_file;
  string proto_file;
  string weight_file;
  if (!GetConfigs(config_file, proto_file, weight_file)) {
    return false;
  }
  AINFO << "--    config_file: " << config_file;
  AINFO << "--     proto_file: " << proto_file;
  AINFO << "--    weight_file: " << weight_file;

  if (!apollo::common::util::GetProtoFromFile(config_file, &cnnseg_param_)) {
    AERROR << "Failed to load config file of CNNSegmentation.";
  }

  /// set parameters
  apollo::perception::cnnseg::NetworkParam network_param = cnnseg_param_.network_param();
  apollo::perception::cnnseg::FeatureParam feature_param = cnnseg_param_.feature_param();

  range_ = static_cast<float>(feature_param.point_cloud_range());
  width_ = static_cast<int>(feature_param.width());
  height_ = static_cast<int>(feature_param.height());

  /// Instantiate Caffe net
#ifdef CPU_ONLY
  caffe::Caffe::set_mode(caffe::Caffe::CPU);
#else
  int gpu_id = static_cast<int>(cnnseg_param_.gpu_id());
  CHECK(gpu_id >= 0);
  caffe::Caffe::SetDevice(gpu_id);
  caffe::Caffe::set_mode(caffe::Caffe::GPU);
  caffe::Caffe::DeviceQuery();
#endif

  caffe_net_.reset(new caffe::Net<float>(proto_file, caffe::TEST));
  caffe_net_->CopyTrainedLayersFrom(weight_file);

  AERROR << "confidence threshold = "<<cnnseg_param_.confidence_thresh();
  
#ifdef CPU_ONLY  
  AERROR << "using Caffe CPU mode";
#else
  AERROR << "using Caffe GPU mode";
#endif    

  /// set related Caffe blobs
  // center offset prediction
  instance_pt_blob_ = caffe_net_->blob_by_name(network_param.instance_pt_blob());
  CHECK(instance_pt_blob_ != nullptr) << "`" << network_param.instance_pt_blob()
                                      << "` not exists!";
  // objectness prediction
  category_pt_blob_ = caffe_net_->blob_by_name(network_param.category_pt_blob());
  CHECK(category_pt_blob_ != nullptr) << "`" << network_param.category_pt_blob()
                                      << "` not exists!";
  // positiveness (foreground probability) prediction
  confidence_pt_blob_ = caffe_net_->blob_by_name(network_param.confidence_pt_blob());
  CHECK(confidence_pt_blob_ != nullptr) << "`" << network_param.confidence_pt_blob()
                                        << "` not exists!";
  // object height prediction
  height_pt_blob_ = caffe_net_->blob_by_name(network_param.height_pt_blob());
  CHECK(height_pt_blob_ != nullptr) << "`" << network_param.height_pt_blob()
                                   << "` not exists!";
  // raw feature data
  feature_blob_ = caffe_net_->blob_by_name(network_param.feature_blob());
  CHECK(feature_blob_ != nullptr) << "`" << network_param.feature_blob()
                                  << "` not exists!";

  cluster2d_.reset(new cnnseg::Cluster2D());
  if (!cluster2d_->Init(height_, width_, range_)) {
    AERROR << "Fail to init cluster2d for CNNSegmentation";
  }

  feature_generator_.reset(new cnnseg::FeatureGenerator<float>());
  if (!feature_generator_->Init(feature_param, feature_blob_.get())) {
    AERROR << "Fail to init feature generator for CNNSegmentation";
    return false;
  }

  return true;
}

bool CNNSegmentation::Segment(const pcl_util::PointCloudPtr& pc_ptr,
                              const pcl_util::PointIndices& valid_indices,
                              const SegmentationOptions& options,
                              vector<ObjectPtr>* objects) {
  objects->clear();
  int num_pts = static_cast<int>(pc_ptr->points.size());
  if (num_pts == 0) {
    AINFO << "None of input points, return directly.";
    return true;
  }

  use_full_cloud_ = cnnseg_param_.use_full_cloud() && (options.origin_cloud != nullptr);
  timer_.Tic();

  // generate raw features
  if (use_full_cloud_) {
    feature_generator_->Generate(options.origin_cloud);
  } else {
    feature_generator_->Generate(pc_ptr);
  }
  feat_time_ = timer_.Toc(true);

  // network forward process
#ifndef CPU_ONLY   
  caffe::Caffe::set_mode(caffe::Caffe::GPU);
#endif  
  caffe_net_->Forward();
  network_time_ = timer_.Toc(true);
  
  // clutser points and construct segments/objects
  cluster2d_->Cluster(*category_pt_blob_, *instance_pt_blob_,
                      pc_ptr, valid_indices,
                      cnnseg_param_.objectness_thresh(),
                      cnnseg_param_.use_all_grids_for_clustering());
  clust_time_ = timer_.Toc(true);

  cluster2d_->Filter(*confidence_pt_blob_, *height_pt_blob_);
  cluster2d_->GetObjects(cnnseg_param_.confidence_thresh(),
                         cnnseg_param_.height_thresh(),
                         cnnseg_param_.min_pts_num(),
                         objects);
  post_time_ = timer_.Toc(true);

  tot_time_ = feat_time_ + network_time_ + clust_time_ + post_time_;

  AERROR
      << "Total runtime: " << tot_time_ << "ms\t"
      << "  Feature generation: " << feat_time_ << "ms\t"
      << "  CNN forward: " << network_time_ << "ms\t"
      << "  Clustering: " << clust_time_ << "ms\t"
      << "  Post-processing: " << post_time_ << "ms";

  return true;
}

bool CNNSegmentation::GetConfigs(string& config_file,
                                 string& proto_file,
                                 string& weight_file) {
  ConfigManager *config_manager = Singleton<ConfigManager>::Get();
  CHECK_NOTNULL(config_manager);

  const ModelConfig *model_config = nullptr;
  if (!config_manager->GetModelConfig("CNNSegmentation", &model_config)) {
    AERROR << "Failed to get model config for CNNSegmentation";
    return false;
  }
  const string &work_root = config_manager->work_root();

  if (!model_config->GetValue("config_file", &config_file)) {
    AERROR << "Failed to get value of config_file.";
    return false;
  }
  config_file = FileUtil::GetAbsolutePath(work_root, config_file);

  if (!model_config->GetValue("proto_file", &proto_file)) {
    AERROR << "Failed to get value of proto_file.";
    return false;
  }
  proto_file = FileUtil::GetAbsolutePath(work_root, proto_file);

  if (!model_config->GetValue("weight_file", &weight_file)) {
    AERROR << "Failed to get value of weight_file.";
    return false;
  }
  weight_file = FileUtil::GetAbsolutePath(work_root, weight_file);

  return true;
}

}  // namespace perception
}  // namespace apollo
