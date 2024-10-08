// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QString>

#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules::utils {

QString eventType(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString eventCaption(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString eventName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString eventDescription(
    const AggregatedEventPtr& eventAggregator, common::SystemContext* context);

// Keep in sync with StringHelper::eventDescription().
QString extendedEventDescription(
    const AggregatedEventPtr& eventAggregator, common::SystemContext* context);

QString eventTime(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString eventTimeStart(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString eventTimeEnd(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
template<class T>
QString eventTimestamp(const AggregatedEventPtr& eventAggregator, common::SystemContext*)
{
    const auto count = std::chrono::duration_cast<T>(eventAggregator->timestamp()).count();
    return QString::fromStdString(reflect::toString(count));
}
QString eventSource(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);

QString deviceIp(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString deviceId(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString deviceMac(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString deviceType(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);

QString deviceName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);

QString siteName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString userName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);
QString eventAttribute(const QString& attributeName, const AggregatedEventPtr& eventAggregator);
QString serverName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context);

} // namespace nx::vms::rules::utils
