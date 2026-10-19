// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/top_posts/top_posts_inner.h"

#include "base/unixtime.h"
#include "data/data_peer.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "window/window_session_controller.h"
#include "lang_auto.h"
#include <QtCore/QDateTime>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

#include "styles/style_ayu_styles.h"
#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_window.h"

namespace TopPosts {
namespace {

[[nodiscard]] QString FormatCount(int count) {
	if (count < 1000) {
		return QString::number(count);
	} else if (count < 1000000) {
		auto val = count / 1000.0;
		auto formatted = QString::number(val, 'f', 1);
		if (formatted.endsWith(u".0"_q)) {
			formatted.chop(2);
		}
		return formatted + u"K"_q;
	} else {
		auto val = count / 1000000.0;
		auto formatted = QString::number(val, 'f', 1);
		if (formatted.endsWith(u".0"_q)) {
			formatted.chop(2);
		}
		return formatted + u"M"_q;
	}
}

[[nodiscard]] QString ExtractReactionEmoji(const std::string &json) {
	if (json.empty() || json == "{}") {
		return u"🔥"_q;
	}
	auto start = json.find('"');
	if (start == std::string::npos) {
		return u"🔥"_q;
	}
	auto end = json.find('"', start + 1);
	if (end == std::string::npos) {
		return u"🔥"_q;
	}
	auto emoji = QString::fromUtf8(json.substr(start + 1, end - start - 1));
	if (emoji.isEmpty() || std::all_of(emoji.begin(), emoji.end(), [](QChar c) { return c.isDigit(); })) {
		return u"🔥"_q;
	}
	return emoji;
}

struct MediaTypeMetadata {
	QString placeholder;
	QString icon;
};

[[nodiscard]] MediaTypeMetadata GetMediaTypeMetadata(
		TopPostMediaType type,
		int groupedCount = 1) {
	switch (type) {
	case TopPostMediaType::Photo:
		return { u"[Photo]"_q, u"🖼"_q };
	case TopPostMediaType::Video:
		return { u"[Video]"_q, u"🎬"_q };
	case TopPostMediaType::Document:
		return { u"[Document]"_q, u"📄"_q };
	case TopPostMediaType::Grouped: {
		const auto placeholder = (groupedCount > 1)
			? u"[Album / Media Set (%1)]"_q.arg(groupedCount)
			: u"[Album / Media Set]"_q;
		return { placeholder, u"📚"_q };
	}
	case TopPostMediaType::None:
	default:
		return { u"[Message]"_q, u"💬"_q };
	}
}

[[nodiscard]] QString FormatMediaPlaceholder(TopPostMediaType type, int groupedCount = 1) {
	return GetMediaTypeMetadata(type, groupedCount).placeholder;
}

[[nodiscard]] QString MediaIconSymbol(TopPostMediaType type) {
	return GetMediaTypeMetadata(type).icon;
}

} // namespace

InnerWidget::InnerWidget(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer)
: Ui::RpWidget(parent)
, _controller(controller)
, _peer(peer) {
	setAttribute(Qt::WA_OpaquePaintEvent);
	setMouseTracking(true);
}

void InnerWidget::setPosts(std::vector<ChannelTopPost> posts) {
	_posts = std::move(posts);
	_hoveredIndex = -1;
	resizeGetHeight(width());
	update();
}

void InnerWidget::setFilter(AyuDatabase::TopPostsFilter filter) {
	if (_filter != filter) {
		_filter = filter;
		_hoveredIndex = -1;
		update();
	}
}

void InnerWidget::setSortOrder(AyuDatabase::TopPostsSortOrder sortOrder) {
	if (_sortOrder != sortOrder) {
		_sortOrder = sortOrder;
		_hoveredIndex = -1;
		update();
	}
}

void InnerWidget::setEmptyMessage(const QString &message) {
	_emptyMessage = message;
	update();
}

void InnerWidget::setVisibleTopBottom(int visibleTop, int visibleBottom) {
	_visibleTop = visibleTop;
	_visibleBottom = visibleBottom;
	update();
}

void InnerWidget::visibleTopBottomUpdated(int visibleTop, int visibleBottom) {
	_visibleTop = visibleTop;
	_visibleBottom = visibleBottom;
}

int InnerWidget::resizeGetHeight(int newWidth) {
	if (_posts.empty()) {
		return st::topPostsEmptyWidth;
	}
	const auto count = static_cast<int>(_posts.size());
	const auto itemHeight = st::topPostsCardHeight + st::topPostsCardMargin;
	const auto totalHeight = count * itemHeight + st::topPostsCardMargin;
	return std::max(totalHeight, st::topPostsEmptyWidth);
}

int InnerWidget::cardIndexFromPosition(QPoint pos) const {
	if (_posts.empty()) {
		return -1;
	}
	const auto itemHeight = st::topPostsCardHeight + st::topPostsCardMargin;
	const auto index = (pos.y() - st::topPostsCardMargin) / itemHeight;
	if (index >= 0 && index < static_cast<int>(_posts.size())) {
		const auto rect = cardRect(index);
		if (rect.contains(pos)) {
			return index;
		}
	}
	return -1;
}

QRect InnerWidget::cardRect(int index) const {
	const auto itemHeight = st::topPostsCardHeight + st::topPostsCardMargin;
	const auto y = index * itemHeight + st::topPostsCardMargin;
	const auto w = width() - 2 * st::topPostsCardMargin;
	return QRect(st::topPostsCardMargin, y, w, st::topPostsCardHeight);
}

void InnerWidget::mouseMoveEvent(QMouseEvent *e) {
	const auto newHovered = cardIndexFromPosition(e->pos());
	if (_hoveredIndex != newHovered) {
		_hoveredIndex = newHovered;
		setCursor((_hoveredIndex >= 0) ? style::cur_pointer : style::cur_default);
		update();
	}
}

void InnerWidget::leaveEvent(QEvent *e) {
	if (_hoveredIndex != -1) {
		_hoveredIndex = -1;
		setCursor(style::cur_default);
		update();
	}
}

void InnerWidget::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		const auto clicked = cardIndexFromPosition(e->pos());
		if (clicked >= 0 && clicked < static_cast<int>(_posts.size())) {
			const auto &post = _posts[clicked];
			_clicks.fire_copy(post.messageId);
		}
	}
}

void InnerWidget::paintEvent(QPaintEvent *e) {
	Painter p(this);
	const auto clip = e->rect();
	p.fillRect(clip, st::windowBg);

	if (_posts.empty()) {
		paintEmpty(p);
		return;
	}

	for (auto i = 0; i < static_cast<int>(_posts.size()); ++i) {
		const auto rect = cardRect(i);
		if (rect.bottom() < clip.top() || rect.top() > clip.bottom()) {
			continue;
		}
		paintCard(p, _posts[i], rect, (i == _hoveredIndex));
	}
}

void InnerWidget::paintEmpty(QPainter &p) {
	const auto rect = QRect(
		(width() - st::topPostsEmptyWidth) / 2,
		st::topPostsBannerHeight + st::topPostsTabHeight,
		st::topPostsEmptyWidth,
		st::topPostsEmptyWidth);

	p.setFont(st::normalFont);
	p.setPen(st::windowSubTextFg);
	const auto text = _emptyMessage.isEmpty()
		? tr::ayu_TopPostsEmptyMatching(tr::now)
		: _emptyMessage;
	p.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, text);
}

void InnerWidget::paintCard(
		QPainter &p,
		const ChannelTopPost &post,
		const QRect &rect,
		bool hovered) {
	if (hovered) {
		p.setPen(Qt::NoPen);
		p.setBrush(st::windowBgOver);
		p.drawRoundedRect(rect, st::topPostsThumbRounding, st::topPostsThumbRounding);
	}

	const auto thumbX = rect.x() + st::topPostsCardPadding.left();
	const auto thumbY = rect.y() + (rect.height() - st::topPostsThumbSize) / 2;
	const auto thumbRect = QRect(thumbX, thumbY, st::topPostsThumbSize, st::topPostsThumbSize);

	p.setPen(Qt::NoPen);
	p.setBrush(st::msgServiceBg);
	p.drawRoundedRect(thumbRect, st::topPostsThumbRounding, st::topPostsThumbRounding);

	p.setFont(st::semiboldFont);
	p.setPen(st::msgServiceFg);
	const auto icon = MediaIconSymbol(post.type());
	p.drawText(thumbRect, Qt::AlignCenter, icon);

	const auto dateText = QDateTime::fromSecsSinceEpoch(post.date).toString(u"MMM d, yyyy"_q);
	p.setFont(st::normalFont);
	p.setPen(st::windowSubTextFg);
	const auto dateWidth = p.fontMetrics().horizontalAdvance(dateText);
	const auto dateX = rect.x() + rect.width() - st::topPostsCardDateRight - dateWidth;
	const auto dateY = rect.y() + st::topPostsCardDateTop + p.fontMetrics().ascent();
	p.drawText(dateX, dateY, dateText);

	const auto textLeft = rect.x() + st::topPostsCardTextLeft;
	const auto textTop = rect.y() + st::topPostsCardTextTop;
	const auto textMaxW = std::max(0, dateX - textLeft - st::topPostsBadgeMargin);

	auto snippet = QString::fromUtf8(post.textSnippet);
	if (snippet.trimmed().isEmpty()) {
		snippet = FormatMediaPlaceholder(post.type(), post.groupedCount);
	}
	const auto elided = p.fontMetrics().elidedText(snippet, Qt::ElideRight, textMaxW);
	p.setFont(st::semiboldFont);
	p.setPen(st::windowFg);
	p.drawText(textLeft, textTop + p.fontMetrics().ascent(), elided);

	auto badgeX = textLeft;
	const auto badgeY = rect.y() + st::topPostsCardBadgeTop;

	const auto drawBadge = [&](const QString &label, const style::color &bg, const style::color &fg) {
		p.setFont(st::normalFont);
		const auto labelW = p.fontMetrics().horizontalAdvance(label);
		const auto badgeW = labelW + st::topPostsBadgePadding.left() + st::topPostsBadgePadding.right();
		const auto bRect = QRect(badgeX, badgeY, badgeW, st::topPostsBadgeHeight);

		p.setPen(Qt::NoPen);
		p.setBrush(bg);
		p.drawRoundedRect(bRect, st::topPostsBadgeRounding, st::topPostsBadgeRounding);

		p.setPen(fg);
		p.drawText(
			bRect.x() + st::topPostsBadgePadding.left(),
			bRect.y() + st::topPostsBadgePadding.top() + p.fontMetrics().ascent(),
			label);

		badgeX += badgeW + st::topPostsBadgeMargin;
	};

	const auto emoji = ExtractReactionEmoji(post.reactionsJson);
	const auto reactionsLabel = emoji + u" "_q + FormatCount(post.reactionsCount);
	drawBadge(reactionsLabel, st::msgServiceBg, st::msgServiceFg);

	if (post.commentsCount > 0) {
		const auto commentsLabel = u"💬 "_q + FormatCount(post.commentsCount);
		drawBadge(commentsLabel, st::msgServiceBg, st::msgServiceFg);
	}

	if (post.viewsCount > 0) {
		const auto viewsLabel = u"👁 "_q + FormatCount(post.viewsCount);
		drawBadge(viewsLabel, st::msgServiceBg, st::windowSubTextFg);
	}

	if (post.isGrouped() && post.groupedCount > 1) {
		const auto groupLabel = u"📦 %1"_q.arg(post.groupedCount);
		drawBadge(groupLabel, st::msgServiceBg, st::msgServiceFg);
	}
}

} // namespace TopPosts
