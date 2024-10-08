// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "optional_duration_picker_widget.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSpinBox>

#include <nx/vms/rules/utils/field.h>
#include <ui/common/read_only.h>
#include <ui/widgets/common/elided_label.h>

#include "common.h"

namespace nx::vms::client::desktop::rules {

OptionalDurationPicker::OptionalDurationPicker(
    vms::rules::OptionalTimeField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    TitledFieldPickerWidget<vms::rules::OptionalTimeField>(field, context, parent)
{
    auto mainLayout = new QHBoxLayout;

    m_label = new QnElidedLabel;
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setElideMode(Qt::ElideRight);

    mainLayout->addWidget(m_label);

    auto timeWidgetLayout = new QHBoxLayout;

    m_timeDurationWidget = new TimeDurationWidget;
    m_timeDurationWidget->valueSpinBox()->setFixedWidth(kDurationValueSpinBoxWidth);
    timeWidgetLayout->addWidget(m_timeDurationWidget);

    timeWidgetLayout->addStretch();

    mainLayout->addLayout(timeWidgetLayout);

    mainLayout->setStretch(0, 1);
    mainLayout->setStretch(1, 5);

    m_contentWidget->setLayout(mainLayout);

    const auto fieldDescriptor = field->descriptor();
    if (fieldDescriptor->fieldName == vms::rules::utils::kIntervalFieldName)
        m_label->setText(tr("Once in"));
    else if (fieldDescriptor->fieldName == vms::rules::utils::kPlaybackTimeFieldName)
        m_label->setText(tr("For"));
    else
        m_label->setText(tr("Value"));

    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Hours);
    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Days);

    const auto fieldProperties = field->properties();
    m_timeDurationWidget->setMaximum(static_cast<int>(fieldProperties.maximumValue.count()));
    // The minimum value must be greater than zero. If the user wishes to set a value of zero,
    // there is a corresponding switch control in the provided widget.
    m_timeDurationWidget->setMinimum(
        std::max(static_cast<int>(fieldProperties.minimumValue.count()), 1));

    connect(
        m_timeDurationWidget,
        &TimeDurationWidget::valueChanged,
        this,
        [this]
        {
            m_field->setValue(std::chrono::seconds{m_timeDurationWidget->value()});
        });
}

void OptionalDurationPicker::updateUi()
{
    TitledFieldPickerWidget<vms::rules::OptionalTimeField>::updateUi();

    if (m_field->descriptor()->fieldName == vms::rules::utils::kIntervalFieldName)
    {
        const auto durationField =
            getActionField<vms::rules::OptionalTimeField>(vms::rules::utils::kDurationFieldName);
        if (durationField)
        {
            // If the action has duration field, 'Interval of action' picker must be visible only
            // when duration value is non zero.
            setVisible(durationField->value() != vms::rules::OptionalTimeField::value_type::zero());
        }
    }

    {
        QSignalBlocker blocker{m_timeDurationWidget};
        m_timeDurationWidget->setValue(
            std::chrono::duration_cast<std::chrono::seconds>(m_field->value()).count());
    }

    setChecked(m_field->value() != vms::rules::OptionalTimeField::value_type::zero());
}

void OptionalDurationPicker::onEnabledChanged(bool isEnabled)
{
    TitledFieldPickerWidget<vms::rules::OptionalTimeField>::onEnabledChanged(isEnabled);

    if (isEnabled)
    {
        auto newValue = std::max(m_field->properties().value, m_field->properties().minimumValue);
        if (newValue == std::chrono::seconds::zero())
            newValue = std::chrono::seconds{1};

        m_field->setValue(newValue);
    }
    else
    {
        m_field->setValue(vms::rules::OptionalTimeField::value_type::zero());
    }

    setEdited();
}

bool OptionalDurationPicker::hasSeparator()
{
    return true;
}

} // namespace nx::vms::client::desktop::rules
