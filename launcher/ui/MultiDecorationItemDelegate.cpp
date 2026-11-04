#include <QApplication>
#include <QIcon>
#include <QModelIndex>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QTextDocument>
#include <Qt>

#include "MultiDecorationItemDelegate.h"
#include "widgets/InfoFrame.h"

namespace {
std::optional<QList<MultiDecorationItemDelegate::Icon>> iconsAt(const QModelIndex& index)
{
    QVariant val = index.data(Qt::DecorationRole);
    if (val.metaType() != QMetaType::fromType<QList<MultiDecorationItemDelegate::Icon>>()) {
        return std::nullopt;
    }
    return val.value<QList<MultiDecorationItemDelegate::Icon>>();
}

int iconSpacing(const QStyleOptionViewItem& opt)
{
    QStyle* style = opt.widget != nullptr ? opt.widget->style() : QApplication::style();
    return (style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) + 1) * 2;
}
}  // namespace

void MultiDecorationItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    const int spacing = iconSpacing(*option);

    const auto icons = iconsAt(index);
    if (!icons.has_value()) {
        // leave it as-is (so QIcon etc. can be used)
        return;
    }
    if (icons->isEmpty()) {
        option->features &= ~QStyleOptionViewItem::HasDecoration;
        return;
    }

    int width = 0;
    for (const auto& [icon, size] : icons.value()) {
        if (width != 0) {
            width += spacing;
        }
        width += size.width();
    }

    // HACK: offset the drawing of the text so we can do our own custom drawing
    option->decorationSize = { width, 1 };
}

void MultiDecorationItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    const int spacing = iconSpacing(option);

    const auto icons = iconsAt(index);
    if (!icons.has_value()) {
        return;
    }

    int x = 0;
    for (const auto& [icon, size] : icons.value()) {
        if (x != 0) {
            x += spacing;
        }

        QSize actualSize = icon.actualSize(size);

        const QIcon::Mode mode = (option.state & QStyle::State_Selected) != 0 ? QIcon::Selected : QIcon::Normal;
        const QIcon::State state = (option.state & QStyle::State_Open) != 0 ? QIcon::On : QIcon::Off;

        // NOTE: the icon may be smaller than the size we've requested
        // if so, we'll want to centre it but still use the same amount of space
        int iconX = (size.width() - actualSize.width()) / 2;
        int iconY = (option.rect.height() - actualSize.height()) / 2;

        icon.paint(painter, option.rect.x() + x + iconX, option.rect.y() + iconY, actualSize.width(), actualSize.height(),
                   Qt::AlignLeft | Qt::AlignTop, mode, state);
        x += size.width();
    }
}

QSize MultiDecorationItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto sizeHint = index.data(Qt::SizeHintRole).value<QSize>();
    if (sizeHint.isValid()) {
        return sizeHint;
    }

    // HACK: if there's no size hint, always pretend we have a decoration when determining height (so it remains consistent)
    // we shouldn't do this when determining width as there's a little bit of extra padding added before the text which we don't want
    auto opt = option;
    initStyleOption(&opt, index);

    auto altOpt = opt;
    altOpt.features |= QStyleOptionViewItem::HasDecoration;

    const auto* style = opt.widget != nullptr ? opt.widget->style() : QApplication::style();

    int width = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, {}, opt.widget).width();
    int height = style->sizeFromContents(QStyle::CT_ItemViewItem, &altOpt, {}, opt.widget).height();
    return {width, height};
}

void ColorCodeItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    MultiDecorationItemDelegate::initStyleOption(option, index);
    if (index.column() == m_column) {
        option->text.clear();
    }
}

void ColorCodeItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    MultiDecorationItemDelegate::paint(painter, option, index);
    if (index.column() != m_column) {
        return;
    }

    auto textOption = option;
    initStyleOption(&textOption, index);

    const auto* style = textOption.widget != nullptr ? textOption.widget->style() : QApplication::style();
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &textOption, textOption.widget);

    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(textOption.font);
    const auto colorRole = (textOption.state & QStyle::State_Selected) != 0 ? QPalette::HighlightedText : QPalette::Text;
    document.setDefaultStyleSheet(QString("body { color: %1; }").arg(textOption.palette.color(colorRole).name()));
    document.setHtml(InfoFrame::renderColorCodes(index.data(Qt::DisplayRole).toString()));
    document.setTextWidth(textRect.width());

    painter->save();
    painter->setClipRect(textRect);
    const qreal yOffset = qMax<qreal>(0, (textRect.height() - document.size().height()) / 2);
    painter->translate(textRect.left(), textRect.top() + yOffset);
    document.drawContents(painter);
    painter->restore();
}
