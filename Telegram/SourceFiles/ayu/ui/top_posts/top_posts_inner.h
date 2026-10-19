// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/data/ayu_database.h"
#include "ayu/data/entities.h"
#include "ui/rp_widget.h"
#include "ui/text/text.h"

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace TopPosts {

class InnerWidget final : public Ui::RpWidget {
public:
	InnerWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer);

	[[nodiscard]] not_null<PeerData*> peer() const {
		return _peer;
	}

	void setPosts(std::vector<ChannelTopPost> posts);
	[[nodiscard]] const std::vector<ChannelTopPost> &posts() const {
		return _posts;
	}

	void setFilter(AyuDatabase::TopPostsFilter filter);
	void setSortOrder(AyuDatabase::TopPostsSortOrder sortOrder);
	void setEmptyMessage(const QString &message);
	void setVisibleTopBottom(int visibleTop, int visibleBottom);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void leaveEvent(QEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	int resizeGetHeight(int newWidth) override;
	void visibleTopBottomUpdated(int visibleTop, int visibleBottom) override;

private:
	[[nodiscard]] int cardIndexFromPosition(QPoint pos) const;
	[[nodiscard]] QRect cardRect(int index) const;
	void paintCard(
		QPainter &p,
		const ChannelTopPost &post,
		const QRect &rect,
		bool hovered);
	void paintEmpty(QPainter &p);

	const not_null<Window::SessionController*> _controller;
	const not_null<PeerData*> _peer;

	std::vector<ChannelTopPost> _posts;
	AyuDatabase::TopPostsFilter _filter = AyuDatabase::TopPostsFilter::All;
	AyuDatabase::TopPostsSortOrder _sortOrder = AyuDatabase::TopPostsSortOrder::Reactions;

	int _hoveredIndex = -1;
	int _visibleTop = 0;
	int _visibleBottom = 0;
	QString _emptyMessage;

};

} // namespace TopPosts
