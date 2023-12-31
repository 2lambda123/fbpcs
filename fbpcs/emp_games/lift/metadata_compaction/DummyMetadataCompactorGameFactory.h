/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fbpcs/emp_games/lift/metadata_compaction/DummyMetadataCompactorGame.h"
#include "fbpcs/emp_games/lift/metadata_compaction/IMetadataCompactorGameFactory.h"

namespace private_lift {

template <int schedulerId>
class DummyMetadataCompactorGameFactory
    : public IMetadataCompactorGameFactory<schedulerId> {
 public:
  std::unique_ptr<IMetadataCompactorGame<schedulerId>> create(
      std::unique_ptr<fbpcf::scheduler::IScheduler> scheduler,
      int partyId) {
    return std::make_unique<DummyMetadataCompactorGame<schedulerId>>(
        partyId, std::move(scheduler));
  }
};

} // namespace private_lift
