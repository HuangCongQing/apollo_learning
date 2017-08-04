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

/**
 * @file qp_spline_path_generator.cc
 **/
#include <algorithm>
#include <utility>
#include <vector>

#include "modules/planning/optimizer/qp_spline_path/qp_spline_path_generator.h"

#include "modules/common/proto/pnc_point.pb.h"

#include "modules/common/log.h"
#include "modules/common/macro.h"
#include "modules/common/util/string_util.h"
#include "modules/common/util/util.h"
#include "modules/planning/common/planning_gflags.h"
#include "modules/planning/math/double.h"
#include "modules/planning/math/sl_analytic_transformation.h"

namespace apollo {
namespace planning {

QpSplinePathGenerator::QpSplinePathGenerator(
    const ReferenceLine& reference_line,
    const QpSplinePathConfig& qp_spline_path_config)
    : reference_line_(reference_line),
      qp_spline_path_config_(qp_spline_path_config) {}

bool QpSplinePathGenerator::Generate(const DecisionData& decision_data,
                                     const SpeedData& speed_data,
                                     const common::TrajectoryPoint& init_point,
                                     PathData* const path_data) {
  if (!CalculateInitFrenetPoint(init_point, &init_frenet_point_)) {
    AERROR << "Fail to map init point: " << init_point.ShortDebugString();
    return false;
  }
  double start_s = init_frenet_point_.s();
  double end_s = std::min(reference_line_.length(),
                          init_frenet_point_.s() + FLAGS_planning_distance);

  QpFrenetFrame qp_frenet_frame(reference_line_, decision_data, speed_data,
                                init_frenet_point_, start_s, end_s,
                                qp_spline_path_config_.time_resolution());
  if (!qp_frenet_frame.Init(qp_spline_path_config_.num_output())) {
    AERROR << "Fail to initialize qp frenet frame";
    return false;
  }

  if (!InitCoordRange(qp_frenet_frame, &start_s, &end_s)) {
    AERROR << "Measure natural coord system with s range failed!";
    return false;
  }

  AINFO << "pss path start with " << start_s << ", end with " << end_s;
  if (!InitSpline(init_frenet_point_, start_s, end_s - 0.1)) {
    AERROR << "Init smoothing spline failed with (" << start_s << ",  end_s "
           << end_s;
    return false;
  }

  if (!AddConstraint(qp_frenet_frame)) {
    AERROR << "Fail to setup pss path constraint.";
    return false;
  }
  if (!AddKernel()) {
    AERROR << "Fail to setup pss path kernel.";
    return false;
  }
  if (!Solve()) {
    AERROR << "Fail to solve the qp problem.";
    return false;
  }

  AINFO << common::util::StrCat("Spline dl:", init_frenet_point_.dl(), ", ddl:",
                                init_frenet_point_.ddl());

  // extract data
  const Spline1d& spline = spline_generator_->spline();
  std::vector<common::PathPoint> path_points;

  double start_l = spline(init_frenet_point_.s());
  ReferencePoint ref_point =
      reference_line_.get_reference_point(init_frenet_point_.s());
  common::math::Vec2d xy_point = SLAnalyticTransformation::calculate_xypoint(
      ref_point.heading(), common::math::Vec2d(ref_point.x(), ref_point.y()),
      start_l);

  double x_diff = xy_point.x() - init_point.path_point().x();
  double y_diff = xy_point.y() - init_point.path_point().y();

  double s = init_frenet_point_.s();
  double s_resolution =
      (end_s - init_frenet_point_.s()) / qp_spline_path_config_.num_output();
  while (Double::compare(s, end_s) < 0) {
    double l = spline(s);
    double dl = spline.derivative(s);
    double ddl = spline.second_order_derivative(s);
    ReferencePoint ref_point = reference_line_.get_reference_point(s);
    common::math::Vec2d xy_point = SLAnalyticTransformation::calculate_xypoint(
        ref_point.heading(), common::math::Vec2d(ref_point.x(), ref_point.y()),
        l);
    xy_point.set_x(xy_point.x() - x_diff);
    xy_point.set_y(xy_point.y() - y_diff);
    double theta = SLAnalyticTransformation::calculate_theta(
        ref_point.heading(), ref_point.kappa(), l, dl);
    double kappa = SLAnalyticTransformation::calculate_kappa(
        ref_point.kappa(), ref_point.dkappa(), l, dl, ddl);
    common::PathPoint path_point = common::util::MakePathPoint(
        xy_point.x(), xy_point.y(), 0.0, theta, kappa, 0.0, 0.0);
    if (path_points.size() != 0) {
      double distance =
          common::util::Distance2D(path_points.back(), path_point);
      path_point.set_s(path_points.back().s() + distance);
    }
    if (Double::compare(path_point.s(), end_s) >= 0) {
      break;
    }
    path_points.push_back(path_point);
    s += s_resolution;
  }
  path_data->set_discretized_path(path_points);
  return true;
}

bool QpSplinePathGenerator::CalculateInitFrenetPoint(
    const common::TrajectoryPoint& traj_point,
    common::FrenetFramePoint* const frenet_frame_point) {
  common::SLPoint sl_point;
  if (!reference_line_.get_point_in_frenet_frame(
          {traj_point.path_point().x(), traj_point.path_point().y()},
          &sl_point)) {
    return false;
  }
  frenet_frame_point->set_s(sl_point.s());
  frenet_frame_point->set_l(sl_point.l());
  const double theta = traj_point.path_point().theta();
  const double kappa = traj_point.path_point().kappa();
  const double l = frenet_frame_point->l();

  ReferencePoint ref_point =
      reference_line_.get_reference_point(frenet_frame_point->s());

  const double theta_ref = ref_point.heading();
  const double kappa_ref = ref_point.kappa();
  const double dkappa_ref = ref_point.dkappa();

  const double dl = SLAnalyticTransformation::calculate_lateral_derivative(
      theta_ref, theta, l, kappa_ref);
  const double ddl =
      SLAnalyticTransformation::calculate_second_order_lateral_derivative(
          theta_ref, theta, kappa_ref, kappa, dkappa_ref, l);
  frenet_frame_point->set_dl(dl);
  frenet_frame_point->set_ddl(ddl);
  return true;
}

bool QpSplinePathGenerator::InitCoordRange(const QpFrenetFrame& qp_frenet_frame,
                                           double* const start_s,
                                           double* const end_s) {
  // TODO(all): step 1 get current sl coordinate - with init coordinate point
  double start_point = std::max(init_frenet_point_.s() - 5.0, 0.0);

  const ReferenceLine& reference_line_ = qp_frenet_frame.GetReferenceLine();

  double end_point =
      std::min(reference_line_.length(), *start_s + FLAGS_planning_distance);

  end_point =
      std::min(qp_frenet_frame.feasible_longitudinal_upper_bound(), end_point);
  *start_s = start_point;
  *end_s = end_point;
  return true;
}

bool QpSplinePathGenerator::InitSpline(
    const common::FrenetFramePoint& init_frenet_point, const double start_s,
    const double end_s) {
  // set knots
  if (qp_spline_path_config_.number_of_knots() <= 1) {
    AERROR << "Two few number of knots: "
           << qp_spline_path_config_.number_of_knots();
    return false;
  }
  double distance = std::fmin(reference_line_.map_path().length(), end_s) -
                    init_frenet_point.s();
  if (distance > FLAGS_planning_distance) {
    distance = FLAGS_planning_distance;
  }
  const double delta_s = distance / qp_spline_path_config_.number_of_knots();
  double curr_knot_s = init_frenet_point.s();

  for (uint32_t i = 0; i <= qp_spline_path_config_.number_of_knots(); ++i) {
    knots_.push_back(curr_knot_s);
    curr_knot_s += delta_s;
  }

  // spawn a new spline generator
  spline_generator_.reset(
      new Spline1dGenerator(knots_, qp_spline_path_config_.spline_order()));

  // set evaluated_s_
  std::uint32_t num_evaluated_s =
      qp_spline_path_config_.number_of_fx_constraint_knots();
  if (num_evaluated_s <= 2) {
    AERROR << "Too few evaluated positions. Suggest: > 2, current number: "
           << num_evaluated_s;
    return false;
  }
  const double ds = (spline_generator_->spline().x_knots().back() -
                     spline_generator_->spline().x_knots().front()) /
                    num_evaluated_s;
  double curr_evaluated_s = spline_generator_->spline().x_knots().front();

  for (uint32_t i = 0; i < num_evaluated_s; ++i) {
    evaluated_s_.push_back(curr_evaluated_s);
    curr_evaluated_s += ds;
  }

  return true;
}

bool QpSplinePathGenerator::AddConstraint(
    const QpFrenetFrame& qp_frenet_frame) {
  Spline1dConstraint* spline_constraint =
      spline_generator_->mutable_spline_constraint();

  // add init status constraint
  spline_constraint->add_point_fx_constraint(init_frenet_point_.s(),
                                             init_frenet_point_.l());
  spline_constraint->add_point_derivative_constraint(init_frenet_point_.s(),
                                                     init_frenet_point_.dl());
  spline_constraint->add_point_second_derivative_constraint(
      init_frenet_point_.s(), init_frenet_point_.ddl());
  AINFO << "init frenet point: " << init_frenet_point_.ShortDebugString();

  // add end point constraint
  spline_constraint->add_point_fx_constraint(knots_.back(), 0.0);
  spline_constraint->add_point_derivative_constraint(knots_.back(), 0.0);
  spline_constraint->add_point_second_derivative_constraint(knots_.back(), 0.0);

  // add map bound constraint
  std::vector<double> boundary_low;
  std::vector<double> boundary_high;
  for (const double s : evaluated_s_) {
    std::pair<double, double> boundary = std::make_pair(0.0, 0.0);
    qp_frenet_frame.GetMapBound(s, &boundary);
    boundary_low.push_back(boundary.first);
    boundary_high.push_back(boundary.second);
  }
  if (!spline_constraint->add_fx_boundary(evaluated_s_, boundary_low,
                                          boundary_high)) {
    AERROR << "Add boundary constraint failed";
    return false;
  }

  // add spline joint third derivative constraint
  if (!spline_constraint->add_third_derivative_smooth_constraint()) {
    AERROR << "Add spline joint third derivative constraint failed!";
    return false;
  }

  return true;
}

bool QpSplinePathGenerator::AddKernel() {
  Spline1dKernel* spline_kernel = spline_generator_->mutable_spline_kernel();

  if (qp_spline_path_config_.regularization_weight() > 0) {
    spline_kernel->add_regularization(
        qp_spline_path_config_.regularization_weight());
  }

  if (qp_spline_path_config_.derivative_weight() > 0) {
    spline_kernel->add_derivative_kernel_matrix(
        qp_spline_path_config_.derivative_weight());
  }

  if (qp_spline_path_config_.second_derivative_weight() > 0) {
    spline_kernel->add_second_order_derivative_matrix(
        qp_spline_path_config_.second_derivative_weight());
  }

  if (qp_spline_path_config_.third_derivative_weight() > 0) {
    spline_kernel->add_third_order_derivative_matrix(
        qp_spline_path_config_.third_derivative_weight());
  }

  // reference line kernel
  if (qp_spline_path_config_.number_of_knots() > 1) {
    std::vector<double> l_vec(knots_.size(), 0.0);
    spline_kernel->add_reference_line_kernel_matrix(
        knots_, l_vec, qp_spline_path_config_.reference_line_weight());
  }
  return true;
}

bool QpSplinePathGenerator::Solve() {
  if (!spline_generator_->solve()) {
    AERROR << "Could not solve the qp problem in spline path generator.";
    return false;
  }
  return true;
}

}  // namespace planning
}  // namespace apollo
