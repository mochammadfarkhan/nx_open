// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "texture_size_helper.h"

#include <QtGui/rhi/qrhi.h>
#include <QtQuick/QQuickWindow>

namespace nx::vms::client::core {

static constexpr int kDefaultMaxSize = 2048;

TextureSizeHelper::TextureSizeHelper(QQuickItem* parent):
    QQuickItem(parent),
    m_maxTextureSize(0)
{
    connect(this, &QQuickItem::windowChanged, this, &TextureSizeHelper::setWindow);
}

int TextureSizeHelper::maxTextureSize() const
{
    return m_maxTextureSize <= 0 ? kDefaultMaxSize : m_maxTextureSize;
}

void TextureSizeHelper::setWindow(QQuickWindow* window)
{
    if (m_window == window)
        return;

    if (m_window)
        m_window->disconnect(this);

    m_window = window;

    if (m_window)
    {
        connect(m_window, &QQuickWindow::beforeSynchronizing, this,
            [this]()
            {
                if (m_maxTextureSize > 0)
                    return;

                if (auto rhi = m_window->rhi())
                    setMaxTextureSize(rhi->resourceLimit(QRhi::TextureSizeMax));
                else
                    setMaxTextureSize(kDefaultMaxSize);
            },
            Qt::DirectConnection);

        connect(m_window, &QQuickWindow::sceneGraphInvalidated, this,
            [this]() { setMaxTextureSize(0); });
    }

    // Avoid emitting maxTextureSizeChanged() signal when window is nullptr
    // because we may be inside the destructor already.
}

void TextureSizeHelper::registerQmlType()
{
    qmlRegisterType<TextureSizeHelper>("nx.vms.client.core", 1, 0, "TextureSizeHelper");
}

void TextureSizeHelper::setMaxTextureSize(int size)
{
    const auto oldSize = std::exchange(m_maxTextureSize, size);
    if (oldSize != size)
        emit maxTextureSizeChanged();
}

} // namespace nx::vms::client::core
