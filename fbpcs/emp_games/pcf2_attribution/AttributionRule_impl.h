/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include "fbpcs/emp_games/common/Constants.h"
#include "fbpcs/emp_games/pcf2_attribution/AttributionRule.h"
#include "fbpcs/emp_games/pcf2_attribution/Constants.h"

namespace pcf2_attribution {

template <int schedulerId>
class LastClickRule : public AttributionRule<schedulerId> {
 public:
  LastClickRule(
      std::int64_t id,
      const std::string& name,
      const std::chrono::seconds& thresholdInSeconds)
      : AttributionRule<schedulerId>(id, name),
        threshold_(thresholdInSeconds) {}

  SecBit<schedulerId> isAttributable(
      const PrivateTouchpoint<schedulerId>& tp,
      const PrivateConversion<schedulerId>& conv,
      const std::vector<SecTimestamp<schedulerId>>& thresholds) const override {
    return (tp.ts < conv.ts) & (conv.ts <= thresholds.at(0));
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPlaintext(
      const Touchpoint& tp) const override {
    std::vector<uint32_t> thresholdNDaysClick;
    for (size_t i = 0; i < tp.ts.size(); ++i) {
      bool isValidClick = tp.isClick.at(i) && (tp.ts.at(i) > 0);
      uint32_t thresholdNDays = tp.ts.at(i) + threshold_.count();
      thresholdNDaysClick.push_back(isValidClick ? thresholdNDays : 0);
    }

    return std::vector<SecTimestamp<schedulerId>>{
        SecTimestamp<schedulerId>(thresholdNDaysClick, common::PUBLISHER)};
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPrivate(
      const PrivateTouchpoint<schedulerId>& privateTp,
      const PrivateIsClick<schedulerId>& privateIsClick,
      size_t batchSize) const override {
    PubTimestamp<schedulerId> zero;
    PubTimestamp<schedulerId> secondsInThreshold;

    zero = PubTimestamp<schedulerId>(std::vector<uint32_t>(batchSize, 0));
    secondsInThreshold = PubTimestamp<schedulerId>(
        std::vector<uint32_t>(batchSize, threshold_.count()));

    auto isValidClick = privateIsClick.isClick & (zero < privateTp.ts);
    auto thresholdNDays = privateTp.ts + secondsInThreshold;
    auto thresholdNDaysClick = zero.mux(isValidClick, thresholdNDays);
    return std::vector<SecTimestamp<schedulerId>>{thresholdNDaysClick};
  }

 private:
  std::chrono::seconds threshold_;
};

template <int schedulerId>
class LastTouch_ClickNDays_ImpressionMDays
    : public AttributionRule<schedulerId> {
 public:
  LastTouch_ClickNDays_ImpressionMDays(
      std::int64_t id,
      const std::string& name,
      std::chrono::seconds clickThreshold,
      std::chrono::seconds impressionThreshold)
      : AttributionRule<schedulerId>(id, name),
        clickThreshold_(clickThreshold),
        impressionThreshold_(impressionThreshold) {}

  /* if click within 28d, if touch within 1d */
  SecBit<schedulerId> isAttributable(
      const PrivateTouchpoint<schedulerId>& tp,
      const PrivateConversion<schedulerId>& conv,
      const std::vector<SecTimestamp<schedulerId>>& thresholds) const override {
    auto validConv = tp.ts < conv.ts;
    auto touchWithinMDays = conv.ts <= thresholds.at(0);
    auto clickWithinNDays = conv.ts <= thresholds.at(1);

    return validConv & (touchWithinMDays | clickWithinNDays);
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPlaintext(
      const Touchpoint& tp) const override {
    std::vector<uint32_t> thresholdMDaysTouch;
    std::vector<uint32_t> thresholdNDaysClick;

    for (size_t i = 0; i < tp.ts.size(); ++i) {
      bool isValid = tp.ts.at(i) > 0;
      bool isValidClick = tp.isClick.at(i) & isValid;

      auto thresholdMDays = tp.ts.at(i) + impressionThreshold_.count();
      thresholdMDaysTouch.push_back(isValid ? thresholdMDays : 0);

      auto thresholdNDays = tp.ts.at(i) + clickThreshold_.count();
      thresholdNDaysClick.push_back(isValidClick ? thresholdNDays : 0);
    }

    return std::vector<SecTimestamp<schedulerId>>{
        SecTimestamp<schedulerId>(thresholdMDaysTouch, common::PUBLISHER),
        SecTimestamp<schedulerId>(thresholdNDaysClick, common::PUBLISHER)};
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPrivate(
      const PrivateTouchpoint<schedulerId>& privateTp,
      const PrivateIsClick<schedulerId>& privateIsClick,
      size_t batchSize) const override {
    PubTimestamp<schedulerId> zero;
    PubTimestamp<schedulerId> secondsInMDays;
    PubTimestamp<schedulerId> secondsInNDays;

    zero = PubTimestamp<schedulerId>(std::vector<uint32_t>(batchSize, 0));
    secondsInMDays = PubTimestamp<schedulerId>(
        std::vector<uint32_t>(batchSize, impressionThreshold_.count()));
    secondsInNDays = PubTimestamp<schedulerId>(
        std::vector<uint32_t>(batchSize, clickThreshold_.count()));

    auto isValid = zero < privateTp.ts;
    auto isValidClick = privateIsClick.isClick & isValid;
    auto thresholdMDays = privateTp.ts + secondsInMDays;
    auto thresholdMDaysTouch = zero.mux(isValid, thresholdMDays);

    auto thresholdNDays = privateTp.ts + secondsInNDays;
    auto thresholdNDaysClick = zero.mux(isValidClick, thresholdNDays);
    return std::vector<SecTimestamp<schedulerId>>{
        thresholdMDaysTouch, thresholdNDaysClick};
  }

 private:
  std::chrono::seconds clickThreshold_;
  std::chrono::seconds impressionThreshold_;
};

/*
  Attribute if the conversion took place within 7 days but
  more than 1 day after the touchpoint
*/
template <int schedulerId>
class LastClick_2_7Days : public AttributionRule<schedulerId> {
 public:
  LastClick_2_7Days()
      : AttributionRule<schedulerId>(
            /* id */ 5,
            /* name */ common::LAST_CLICK_2_7D) {}

  /* if click is within 7d but after 1d */
  SecBit<schedulerId> isAttributable(
      const PrivateTouchpoint<schedulerId>& tp,
      const PrivateConversion<schedulerId>& conv,
      const std::vector<SecTimestamp<schedulerId>>& thresholds) const override {
    auto validConv = tp.ts < conv.ts;
    auto clickAfterOneDay = thresholds.at(0) < conv.ts;
    auto clickWithinSevenDays = conv.ts <= thresholds.at(1);

    return validConv & clickAfterOneDay & clickWithinSevenDays;
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPlaintext(
      const Touchpoint& tp) const override {
    std::vector<uint32_t> lowerBoundOneDayClick;
    std::vector<uint32_t> upperBoundSevenDaysClick;

    for (size_t i = 0; i < tp.ts.size(); ++i) {
      bool isValidClick = tp.isClick.at(i) && (tp.ts.at(i) > 0);
      uint32_t lowerBoundOneDay = tp.ts.at(i) + kSecondsInOneDay;
      uint32_t upperBoundSevenDays = tp.ts.at(i) + kSecondsInSevenDays;

      lowerBoundOneDayClick.push_back(isValidClick ? lowerBoundOneDay : 0);
      upperBoundSevenDaysClick.push_back(
          isValidClick ? upperBoundSevenDays : 0);
    }

    return std::vector<SecTimestamp<schedulerId>>{
        SecTimestamp<schedulerId>(lowerBoundOneDayClick, common::PUBLISHER),
        SecTimestamp<schedulerId>(upperBoundSevenDaysClick, common::PUBLISHER)};
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPrivate(
      const PrivateTouchpoint<schedulerId>& privateTp,
      const PrivateIsClick<schedulerId>& privateIsClick,
      size_t batchSize) const override {
    PubTimestamp<schedulerId> zero;
    PubTimestamp<schedulerId> secondsInOneDay;
    PubTimestamp<schedulerId> secondsInSevenDays;

    zero = PubTimestamp<schedulerId>(std::vector<uint32_t>(batchSize, 0));
    secondsInOneDay = PubTimestamp<schedulerId>(
        std::vector<uint32_t>(batchSize, kSecondsInOneDay));
    secondsInSevenDays = PubTimestamp<schedulerId>(
        std::vector<uint32_t>(batchSize, kSecondsInSevenDays));

    auto isValidClick = privateIsClick.isClick & (zero < privateTp.ts);

    auto lowerBoundOneDay = privateTp.ts + secondsInOneDay;
    auto lowerBoundOneDayClick = zero.mux(isValidClick, lowerBoundOneDay);

    auto upperBoundSevenDay = privateTp.ts + secondsInSevenDays;
    auto upperBoundSevenDayClick = zero.mux(isValidClick, upperBoundSevenDay);

    return std::vector<SecTimestamp<schedulerId>>{
        lowerBoundOneDayClick, upperBoundSevenDayClick};
  }
};

/*
  Attribute to any click in the 2-7D window, favoring the
  most recent. If no such clicks exist, attribute to any
  impression in 1d, favoring the most recent.
*/
template <int schedulerId>
class LastTouch_2_7Days : public AttributionRule<schedulerId> {
 public:
  LastTouch_2_7Days()
      : AttributionRule<schedulerId>(
            /* id */ 6,
            /* name */ common::LAST_TOUCH_2_7D) {}

  SecBit<schedulerId> isAttributable(
      const PrivateTouchpoint<schedulerId>& tp,
      const PrivateConversion<schedulerId>& conv,
      const std::vector<SecTimestamp<schedulerId>>& thresholds) const override {
    auto validConv = tp.ts < conv.ts;
    auto clickAfterOneDay = thresholds.at(0) < conv.ts;
    auto clickWithinSevenDays = conv.ts <= thresholds.at(1);

    auto touchWithinOneDay = conv.ts <= thresholds.at(2);

    return validConv &
        ((clickAfterOneDay & clickWithinSevenDays) | touchWithinOneDay);
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPlaintext(
      const Touchpoint& tp) const override {
    std::vector<uint32_t> lowerBoundOneDayClick;
    std::vector<uint32_t> upperBoundSevenDaysClick;
    std::vector<uint32_t> upperBoundOneDayTouch;

    for (size_t i = 0; i < tp.ts.size(); ++i) {
      bool isValid = tp.ts.at(i) > 0;
      bool isValidClick = tp.isClick.at(i) & isValid;
      uint32_t lowerBoundAndUpperBoundOneDay = tp.ts.at(i) + kSecondsInOneDay;
      uint32_t upperBoundSevenDays = tp.ts.at(i) + kSecondsInSevenDays;

      lowerBoundOneDayClick.push_back(
          isValidClick ? lowerBoundAndUpperBoundOneDay : 0);
      upperBoundSevenDaysClick.push_back(
          isValidClick ? upperBoundSevenDays : 0);
      upperBoundOneDayTouch.push_back(
          (isValid && !isValidClick) ? lowerBoundAndUpperBoundOneDay : 0);
    }

    return std::vector<SecTimestamp<schedulerId>>{
        SecTimestamp<schedulerId>(lowerBoundOneDayClick, common::PUBLISHER),
        SecTimestamp<schedulerId>(upperBoundSevenDaysClick, common::PUBLISHER),
        SecTimestamp<schedulerId>(upperBoundOneDayTouch, common::PUBLISHER)};
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPrivate(
      const PrivateTouchpoint<schedulerId>& privateTp,
      const PrivateIsClick<schedulerId>& privateIsClick,
      size_t batchSize) const override {
    PubTimestamp<schedulerId> zero;
    PubTimestamp<schedulerId> secondsInOneDay;
    PubTimestamp<schedulerId> secondsInSevenDays;

    zero = PubTimestamp<schedulerId>(std::vector<uint32_t>(batchSize, 0));
    secondsInOneDay = PubTimestamp<schedulerId>(
        std::vector<uint32_t>(batchSize, kSecondsInOneDay));
    secondsInSevenDays = PubTimestamp<schedulerId>(
        std::vector<uint32_t>(batchSize, kSecondsInSevenDays));

    auto isValid = zero < privateTp.ts;
    auto isValidClick = privateIsClick.isClick & isValid;

    auto lowerBoundAndUpperBoundOneDay = privateTp.ts + secondsInOneDay;
    auto lowerBoundOneDayClick =
        zero.mux(isValidClick, lowerBoundAndUpperBoundOneDay);

    auto upperBoundSevenDay = privateTp.ts + secondsInSevenDays;
    auto upperBoundSevenDayClick = zero.mux(isValidClick, upperBoundSevenDay);

    auto upperBoundOneDayTouch =
        zero.mux((isValid & !isValidClick), lowerBoundAndUpperBoundOneDay);

    return std::vector<SecTimestamp<schedulerId>>{
        lowerBoundOneDayClick, upperBoundSevenDayClick, upperBoundOneDayTouch};
  }
};

template <int schedulerId>
class LastClick_1Day_TargetId : public AttributionRule<schedulerId> {
 public:
  LastClick_1Day_TargetId()
      : AttributionRule<schedulerId>(
            /* id */ 7,
            /* name */ common::LAST_CLICK_1D_TARGETID) {}

  SecBit<schedulerId> isAttributable(
      const PrivateTouchpoint<schedulerId>& tp,
      const PrivateConversion<schedulerId>& conv,
      const std::vector<SecTimestamp<schedulerId>>& thresholds) const override {
    return (tp.targetId == conv.targetId) & (tp.actionType == conv.actionType) &
        (tp.ts < conv.ts) & (conv.ts <= thresholds.at(0));
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPlaintext(
      const Touchpoint& tp) const override {
    std::vector<uint32_t> thresholdOneDayClick;

    for (size_t i = 0; i < tp.ts.size(); ++i) {
      bool isValidClick = tp.isClick.at(i) && (tp.ts.at(i) > 0);
      uint32_t thresholdOneDay = tp.ts.at(i) + kSecondsInOneDay;
      thresholdOneDayClick.push_back(isValidClick ? thresholdOneDay : 0);
    }

    return std::vector<SecTimestamp<schedulerId>>{
        SecTimestamp<schedulerId>(thresholdOneDayClick, common::PUBLISHER)};
  }

  std::vector<SecTimestamp<schedulerId>> computeThresholdsPrivate(
      const PrivateTouchpoint<schedulerId>& privateTp,
      const PrivateIsClick<schedulerId>& privateIsClick,
      size_t batchSize) const override {
    PubTimestamp<schedulerId> zero;
    PubTimestamp<schedulerId> secondsInOneDay;

    zero = PubTimestamp<schedulerId>(std::vector<uint32_t>(batchSize, 0));
    secondsInOneDay = PubTimestamp<schedulerId>(
        std::vector<uint32_t>(batchSize, kSecondsInOneDay));

    auto isValidClick = privateIsClick.isClick & (zero < privateTp.ts);
    auto thresholdOneDay = privateTp.ts + secondsInOneDay;
    auto thresholdOneDayClick = zero.mux(isValidClick, thresholdOneDay);
    return std::vector<SecTimestamp<schedulerId>>{thresholdOneDayClick};
  }
};

// TODO: remove & replace directly with std::chrono::days once compilation flags
// are fixed.
namespace detail {

auto days = [](std::uint64_t numDays) {
  return std::chrono::seconds(numDays * kSecondsInOneDay);
};

} // namespace detail

template <int schedulerId>
inline const auto SUPPORTED_ATTRIBUTION_RULES =
    std::vector<std::shared_ptr<AttributionRule<schedulerId>>>{
        std::make_shared<LastClickRule<schedulerId>>(
            /* id */ 1,
            /* name */ common::LAST_CLICK_1D,
            detail::days(1)),
        std::make_shared<LastClickRule<schedulerId>>(
            /* id */ 2,
            /* name */ common::LAST_CLICK_28D,
            detail::days(28)),
        std::make_shared<LastTouch_ClickNDays_ImpressionMDays<schedulerId>>(
            /* id */ 3,
            /* name */ common::LAST_TOUCH_1D,
            detail::days(1),
            detail::days(1)),
        std::make_shared<LastTouch_ClickNDays_ImpressionMDays<schedulerId>>(
            /* id */ 4,
            /* name */ common::LAST_TOUCH_28D,
            detail::days(28),
            detail::days(1)),
        std::make_shared<LastClick_2_7Days<schedulerId>>(),
        std::make_shared<LastTouch_2_7Days<schedulerId>>(),
        std::make_shared<LastClick_1Day_TargetId<schedulerId>>()};

template <int schedulerId>
std::shared_ptr<const AttributionRule<schedulerId>>
AttributionRule<schedulerId>::fromNameOrThrow(const std::string& name) {
  for (auto& rule : SUPPORTED_ATTRIBUTION_RULES<schedulerId>) {
    if (rule->name == name) {
      return rule;
    }
  }

  throw std::runtime_error("Unknown attribution rule name: " + name);
}

template <int schedulerId>
std::shared_ptr<const AttributionRule<schedulerId>>
AttributionRule<schedulerId>::fromIdOrThrow(std::int64_t id) {
  for (auto& rule : SUPPORTED_ATTRIBUTION_RULES<schedulerId>) {
    if (rule->id == id) {
      return rule;
    }
  }

  throw std::runtime_error(fmt::format("Unknown attribution id: {}", id));
}
}; // namespace pcf2_attribution
