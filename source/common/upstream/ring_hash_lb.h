#pragma once

#include <vector>

#include "envoy/runtime/runtime.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

#include "common/common/logger.h"
#include "common/upstream/thread_aware_lb_impl.h"

namespace Envoy {
namespace Upstream {

/**
 * All ring hash load balancer stats. @see stats_macros.h
 */
// clang-format off
#define ALL_RING_HASH_LOAD_BALANCER_STATS(GAUGE)                                                   \
  GAUGE(size)                                                                                      \
  GAUGE(min_hashes_per_host)                                                                       \
  GAUGE(max_hashes_per_host)
// clang-format on

/**
 * Struct definition for all ring hash load balancer stats. @see stats_macros.h
 */
struct RingHashLoadBalancerStats {
  ALL_RING_HASH_LOAD_BALANCER_STATS(GENERATE_GAUGE_STRUCT)
};

/**
 * A load balancer that implements consistent modulo hashing ("ketama"). Currently, zone aware
 * routing is not supported. A ring is kept for all hosts as well as a ring for healthy hosts.
 * Unless we are in panic mode, the healthy host ring is used.
 * In the future it would be nice to support:
 * 1) Weighting.
 * 2) Per-zone rings and optional zone aware routing (not all applications will want this).
 * 3) Max request fallback to support hot shards (not all applications will want this).
 */
class RingHashLoadBalancer : public ThreadAwareLoadBalancerBase,
                             Logger::Loggable<Logger::Id::upstream> {
public:
  RingHashLoadBalancer(const PrioritySet& priority_set, ClusterStats& stats, Stats::Scope& scope,
                       Runtime::Loader& runtime, Runtime::RandomGenerator& random,
                       const absl::optional<envoy::api::v2::Cluster::RingHashLbConfig>& config,
                       const envoy::api::v2::Cluster::CommonLbConfig& common_config);

  const RingHashLoadBalancerStats& stats() const { return stats_; }

private:
  using HashFunction = envoy::api::v2::Cluster_RingHashLbConfig_HashFunction;

  struct RingEntry {
    uint64_t hash_;
    HostConstSharedPtr host_;
  };

  struct Ring : public HashingLoadBalancer {
    Ring(const NormalizedHostWeightVector& normalized_host_weights, double min_normalized_weight,
         uint64_t min_ring_size, uint64_t max_ring_size, HashFunction hash_function,
         RingHashLoadBalancerStats& stats);

    // ThreadAwareLoadBalancerBase::HashingLoadBalancer
    HostConstSharedPtr chooseHost(uint64_t hash) const override;

    std::vector<RingEntry> ring_;

    RingHashLoadBalancerStats& stats_;
  };
  typedef std::shared_ptr<const Ring> RingConstSharedPtr;

  // ThreadAwareLoadBalancerBase
  HashingLoadBalancerSharedPtr
  createLoadBalancer(const NormalizedHostWeightVector& normalized_host_weights,
                     double min_normalized_weight, double /* max_normalized_weight */) override {
    return std::make_shared<Ring>(normalized_host_weights, min_normalized_weight, min_ring_size_,
                                  max_ring_size_, hash_function_, stats_);
  }

  static RingHashLoadBalancerStats generateStats(Stats::Scope& scope);

  Stats::ScopePtr scope_;
  RingHashLoadBalancerStats stats_;

  static const uint64_t DefaultMinRingSize = 1024;
  static const uint64_t DefaultMaxRingSize = 1024 * 1024 * 8;
  const uint64_t min_ring_size_;
  const uint64_t max_ring_size_;
  const HashFunction hash_function_;
};

} // namespace Upstream
} // namespace Envoy
