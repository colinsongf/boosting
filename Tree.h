/* Copyright 2015,2016 Tao Xu
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include <boost/scoped_array.hpp>

#include "folly/json.h"
#include "folly/Conv.h"
#include "Config.h"

namespace boosting {

template <class T>
class TreeNode {
 public:
  virtual double eval(const boost::scoped_array<T>& fvec) const = 0;
  virtual void scale(double w) = 0;
  virtual folly::dynamic toJson(const Config& cfg) const = 0;
  virtual ~TreeNode() {}
};

template <class T>
class PartitionNode : public TreeNode<T> {
 public:
  PartitionNode(int fid, T fv)
    : fid_(fid), fv_(fv), fvote_(0.0),
    left_(NULL), right_(NULL) {
  }

  TreeNode<T>* getLeft() const {
    return left_;
  }

  TreeNode<T>* getRight() const {
    return right_;
  }

  int getFid() const {
    return fid_;
  }

  T getFv() const {
    return fv_;
  }

  double getVote() const {
    return fvote_;
  }

  void setLeft(TreeNode<T>* left) {
    left_ = left;
  }

  void setRight(TreeNode<T>* right) {
    right_ = right;
  }

  void setVote(double fvote) {
    fvote_ = fvote;
  }

  double eval(const boost::scoped_array<T>& fvec) const {
    if (fvec[fid_] <= fv_) {
      return left_->eval(fvec);
    } else {
      return right_->eval(fvec);
    }
  }

  void scale(double w) {
    fvote_ *= w;
    left_->scale(w);
    right_->scale(w);
  }

  folly::dynamic toJson(const Config& cfg) const {
    folly::dynamic m = folly::dynamic::object;

    m.insert("index", fid_);
    m.insert("value", fv_);
    m.insert("left", left_->toJson(cfg));
    m.insert("right", right_->toJson(cfg));
    m.insert("vote", fvote_);
    m.insert("feature", cfg.getFeatureName(fid_));
    return m;
  }

  ~PartitionNode() {
    delete left_;
    delete right_;
  }

 private:
  int fid_;
  T fv_;
  double fvote_;

  TreeNode<T>* left_;
  TreeNode<T>* right_;
};

template <class T>
class LeafNode : public TreeNode<T> {
 public:
  explicit LeafNode(double fvote) : fvote_(fvote) {
  }

  double eval(const boost::scoped_array<T>& fvec) const {
    return fvote_;
  }

  double getVote() const {
    return fvote_;
  }

  void scale(double w) {
    fvote_ *= w;
  }

  folly::dynamic toJson(const Config& cfg) const {
    folly::dynamic m = folly::dynamic::object;

    m.insert("index", -1);
    m.insert("vote", fvote_);
    return m;
  }

  ~LeafNode() {
  }

 private:
  double fvote_;
};

// load a regression tree from Json
template <class T>
TreeNode<T>* fromJson(const folly::dynamic& obj, const Config& cfg) {
  const folly::dynamic* feature = nullptr;
  try {
    feature = &obj["feature"];
  } catch(...) {
  }

  double vote = static_cast<T>(obj["vote"].asDouble());

  if (!feature) {
    return new LeafNode<T>(vote);
  } else {
    std::string featureName = feature->asString().toStdString();
    int index = cfg.getFeatureIndex(featureName);
    CHECK_GE(index, 0) << "Failed to find " << featureName << " in config.";
    T value;
    if (obj["value"].isInt()) {
      value = static_cast<T>(obj["value"].asInt());
    } else {
      value = static_cast<T>(obj["value"].asDouble());
    }
    PartitionNode<T>* rt = new PartitionNode<T>(index, value);
    rt->setLeft(fromJson<T>(obj["left"], cfg));
    rt->setRight(fromJson<T>(obj["right"], cfg));
    rt->setVote(vote);
    return rt;
  }
}

template <class T>
  double predict(const std::vector<TreeNode<T>*>& models,
                 const boost::scoped_array<T>& fvec) {

  double f = 0.0;
  for (const auto& m : models) {
    f += m->eval(fvec);
  }
  return f;
}

template <class T>
  double predict_vec(const std::vector<TreeNode<T>*>& models,
                     const boost::scoped_array<T>& fvec,
                     std::vector<double>* score) {

  double f = 0.0;
  for (const auto& m : models) {
    f += m->eval(fvec);
    score->push_back(f);
  }
  return f;
}


}
