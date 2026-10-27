/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ArtflowIcons.h"

#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

#include "ArtflowTheme.h"

namespace {

QByteArray svgSource(const QString &name)
{
    static QHash<QString, QByteArray> cache;
    auto it = cache.constFind(name);
    if (it != cache.constEnd()) return *it;

    QFile file(QStringLiteral(":/artflow/icons/%1.svg").arg(name));
    const QByteArray data = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    cache.insert(name, data);
    return data;
}

class TintedSvgEngine : public QIconEngine
{
public:
    TintedSvgEngine(const QString &name, const QColor &fixed)
        : m_name(name)
        , m_fixed(fixed)
    {
    }

    QIconEngine *clone() const override
    {
        return new TintedSvgEngine(m_name, m_fixed);
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        QByteArray svg = svgSource(m_name);
        svg.replace("#000000", colorFor(mode, state).name(QColor::HexRgb).toLatin1());
        QSvgRenderer renderer(svg);
        renderer.render(painter, rect);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap pixmap(size * scale);
        pixmap.fill(Qt::transparent);
        {
            QPainter painter(&pixmap);
            paint(&painter, QRect(QPoint(0, 0), pixmap.size()), mode, state);
        }
        pixmap.setDevicePixelRatio(scale);
        return pixmap;
    }

private:
    QColor colorFor(QIcon::Mode mode, QIcon::State state) const
    {
        if (mode == QIcon::Disabled) return ArtflowTheme::textFaint();
        if (m_fixed.isValid()) return m_fixed;
        if (state == QIcon::On || mode == QIcon::Selected) return Qt::white;
        if (mode == QIcon::Active) return ArtflowTheme::text();
        return ArtflowTheme::text().darker(118);
    }

    QString m_name;
    QColor m_fixed;
};

}

QIcon ArtflowIcons::icon(const QString &name)
{
    return QIcon(new TintedSvgEngine(name, QColor()));
}

QIcon ArtflowIcons::icon(const QString &name, const QColor &color)
{
    return QIcon(new TintedSvgEngine(name, color));
}
