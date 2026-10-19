// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/top_posts/top_posts_section.h"

#include "ayu/ui/top_posts/top_posts_inner.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "profile/profile_back_button.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/shadow.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "window/window_session_controller.h"
#include <QtGui/QPainter>

#include "styles/style_ayu_styles.h"
#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h"
namespace TopPosts {
namespace {

[[nodiscard]] bool HasDiscussionGroup(not_null<PeerData*> peer) {
	if (const auto channel = peer->asChannel()) {
		return channel->discussionLink() != nullptr;
	}
	return false;
}

} // namespace

class FixedBar final : public Ui::RpWidget {
public:
	FixedBar(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer);

	[[nodiscard]] rpl::producer<> backRequests() const;
	[[nodiscard]] rpl::producer<> refreshRequests() const;
	[[nodiscard]] rpl::producer<int> rescanRequests() const;
	[[nodiscard]] rpl::producer<> clearCacheRequests() const;
	[[nodiscard]] rpl::producer<> cancelRequests() const;
	[[nodiscard]] rpl::producer<AyuDatabase::TopPostsSortOrder> sortChanged() const;
	[[nodiscard]] rpl::producer<AyuDatabase::TopPostsFilter> filterChanged() const;
	[[nodiscard]] rpl::producer<int> heightChanged() const;

	void showBanner(const QString &text, bool canCancel);
	void hideBanner();

protected:
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	void showActionMenu();
	void setupSortButtons();
	void setupFilterButtons();
	void setupBanner();
	void updateButtonsState();

	const not_null<Window::SessionController*> _controller;
	const not_null<PeerData*> _peer;
	const bool _hasDiscussion = false;

	object_ptr<Profile::BackButton> _backButton;
	object_ptr<Ui::IconButton> _menuToggle;
	base::unique_qptr<Ui::PopupMenu> _actionMenu;

	object_ptr<Ui::FlatButton> _reactionsSort;
	object_ptr<Ui::FlatButton> _commentsSort;
	object_ptr<Ui::FlatButton> _viewsSort;

	object_ptr<Ui::FlatButton> _allFilter;
	object_ptr<Ui::FlatButton> _docsFilter;
	object_ptr<Ui::FlatButton> _mediaFilter;

	object_ptr<Ui::RpWidget> _banner;
	object_ptr<Ui::FlatButton> _cancelButton;
	QString _bannerText;
	bool _bannerVisible = false;

	AyuDatabase::TopPostsSortOrder _sortOrder = AyuDatabase::TopPostsSortOrder::Reactions;
	AyuDatabase::TopPostsFilter _filter = AyuDatabase::TopPostsFilter::All;

	rpl::event_stream<> _backRequests;
	rpl::event_stream<> _refreshRequests;
	rpl::event_stream<int> _rescanRequests;
	rpl::event_stream<> _clearCacheRequests;
	rpl::event_stream<> _cancelRequests;
	rpl::event_stream<AyuDatabase::TopPostsSortOrder> _sortChanged;
	rpl::event_stream<AyuDatabase::TopPostsFilter> _filterChanged;
	rpl::event_stream<int> _heightChanged;

};

rpl::producer<> FixedBar::backRequests() const {
	return _backRequests.events();
}

rpl::producer<> FixedBar::refreshRequests() const {
	return _refreshRequests.events();
}

rpl::producer<int> FixedBar::rescanRequests() const {
	return _rescanRequests.events();
}

rpl::producer<> FixedBar::clearCacheRequests() const {
	return _clearCacheRequests.events();
}

rpl::producer<> FixedBar::cancelRequests() const {
	return _cancelRequests.events();
}

rpl::producer<AyuDatabase::TopPostsSortOrder> FixedBar::sortChanged() const {
	return _sortChanged.events();
}

rpl::producer<AyuDatabase::TopPostsFilter> FixedBar::filterChanged() const {
	return _filterChanged.events();
}

rpl::producer<int> FixedBar::heightChanged() const {
	return _heightChanged.events();
}

FixedBar::FixedBar(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer)
: Ui::RpWidget(parent)
, _controller(controller)
, _peer(peer)
, _hasDiscussion(HasDiscussionGroup(peer))
, _backButton(this)
, _menuToggle(this, st::topBarMenuToggle)
, _reactionsSort(this, u"🔥 "_q + tr::ayu_TopPostsSortReactions(tr::now), st::defaultFlatButton)
, _commentsSort(this, u"💬 "_q + tr::ayu_TopPostsSortComments(tr::now), st::defaultFlatButton)
, _viewsSort(this, u"👁 "_q + tr::ayu_TopPostsSortViews(tr::now), st::defaultFlatButton)
, _allFilter(this, tr::ayu_TopPostsFilterAll(tr::now), st::defaultFlatButton)
, _docsFilter(this, tr::ayu_TopPostsFilterDocs(tr::now), st::defaultFlatButton)
, _mediaFilter(this, tr::ayu_TopPostsFilterMedia(tr::now), st::defaultFlatButton)
, _banner(this)
, _cancelButton(_banner.data(), tr::lng_cancel(tr::now), st::defaultFlatButton)
	setAttribute(Qt::WA_OpaquePaintEvent);

	_backButton->moveToLeft(0, 0);
	_backButton->setClickedCallback([=] {
		_backRequests.fire({});
	});

	_menuToggle->setClickedCallback([=] {
		showActionMenu();
	});

	setupSortButtons();
	setupFilterButtons();
	setupBanner();
	updateButtonsState();
}

void FixedBar::setupSortButtons() {
	_reactionsSort->setClickedCallback([=] {
		if (_sortOrder != AyuDatabase::TopPostsSortOrder::Reactions) {
			_sortOrder = AyuDatabase::TopPostsSortOrder::Reactions;
			updateButtonsState();
			_sortChanged.fire_copy(_sortOrder);
		}
	});

	_commentsSort->setClickedCallback([=] {
		if (_hasDiscussion && _sortOrder != AyuDatabase::TopPostsSortOrder::Comments) {
			_sortOrder = AyuDatabase::TopPostsSortOrder::Comments;
			updateButtonsState();
			_sortChanged.fire_copy(_sortOrder);
		}
	});

	if (!_hasDiscussion) {
		_commentsSort->setEnabled(false);
		_commentsSort->setToolTip(tr::ayu_TopPostsNoDiscussionTooltip(tr::now));
	}

	_viewsSort->setClickedCallback([=] {
		if (_sortOrder != AyuDatabase::TopPostsSortOrder::Views) {
			_sortOrder = AyuDatabase::TopPostsSortOrder::Views;
			updateButtonsState();
			_sortChanged.fire_copy(_sortOrder);
		}
	});
}

void FixedBar::setupFilterButtons() {
	_allFilter->setClickedCallback([=] {
		if (_filter != AyuDatabase::TopPostsFilter::All) {
			_filter = AyuDatabase::TopPostsFilter::All;
			updateButtonsState();
			_filterChanged.fire_copy(_filter);
		}
	});

	_docsFilter->setClickedCallback([=] {
		if (_filter != AyuDatabase::TopPostsFilter::Documents) {
			_filter = AyuDatabase::TopPostsFilter::Documents;
			updateButtonsState();
			_filterChanged.fire_copy(_filter);
		}
	});

	_mediaFilter->setClickedCallback([=] {
		if (_filter != AyuDatabase::TopPostsFilter::Media) {
			_filter = AyuDatabase::TopPostsFilter::Media;
			updateButtonsState();
			_filterChanged.fire_copy(_filter);
		}
	});
}

void FixedBar::setupBanner() {
	_banner->hide();
	_cancelButton->setClickedCallback([=] {
		_cancelRequests.fire({});
	});
	_banner->paintRequest(
	) | rpl::on_next([=](const QRect &clip) {
		Painter p(_banner.data());
		p.fillRect(clip, st::msgServiceBg);
		p.setFont(st::normalFont);
		p.setPen(st::msgServiceFg);
		const auto textRect = QRect(
			st::topPostsBannerPadding.left(),
			0,
			_banner->width() - _cancelButton->width() - st::topPostsBannerPadding.left() - st::topPostsBannerPadding.right(),
			_banner->height());
		p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, _bannerText);
	}, _banner->lifetime());
}

void FixedBar::updateButtonsState() {
	update();
}

void FixedBar::showBanner(const QString &text, bool canCancel) {
	_bannerText = text;
	_cancelButton->setVisible(canCancel);
	_bannerVisible = true;
	_banner->show();
	resizeGetHeight(width());
	_heightChanged.fire_copy(height());
	update();
}

void FixedBar::hideBanner() {
	if (_bannerVisible) {
		_bannerVisible = false;
		_banner->hide();
		resizeGetHeight(width());
		_heightChanged.fire_copy(height());
		update();
	}
}

void FixedBar::showActionMenu() {
	_actionMenu = base::make_unique_q<Ui::PopupMenu>(
		_menuToggle.data(),
		st::defaultPopupMenu);
	_actionMenu->addAction(tr::ayu_TopPostsRefresh(tr::now), [=] {
		_refreshRequests.fire({});
	});
	_actionMenu->addAction(tr::ayu_TopPostsRescan500(tr::now), [=] {
		_rescanRequests.fire(500);
	});
	_actionMenu->addAction(tr::ayu_TopPostsRescan1000(tr::now), [=] {
		_rescanRequests.fire(1000);
	});
	_actionMenu->addAction(tr::ayu_TopPostsRescan2000(tr::now), [=] {
		_rescanRequests.fire(2000);
	});
	_actionMenu->addAction(tr::ayu_TopPostsRescan5000(tr::now), [=] {
		_rescanRequests.fire(5000);
	});
	_actionMenu->addAction(tr::ayu_TopPostsClearCache(tr::now), [=] {
		_clearCacheRequests.fire({});
	});
	_actionMenu->setForcedOrigin(Ui::PanelAnimation::Origin::TopRight);
	_actionMenu->popup(_menuToggle->mapToGlobal(QPoint(_menuToggle->width(), _menuToggle->height())));
}

int FixedBar::resizeGetHeight(int newWidth) {
	_menuToggle->moveToRight(st::topPostsTabMargin, (st::topBarHeight - _menuToggle->height()) / 2);

	const auto row2Top = st::topBarHeight;
	const auto halfWidth = (newWidth - 4 * st::topPostsTabMargin) / 2;
	const auto sortBtnWidth = (halfWidth - 2 * st::topPostsTabMargin) / 3;
	const auto filterBtnWidth = (halfWidth - 2 * st::topPostsTabMargin) / 3;

	auto sortX = st::topPostsTabMargin;
	_reactionsSort->setGeometry(sortX, row2Top, sortBtnWidth, st::topPostsTabHeight);
	sortX += sortBtnWidth + st::topPostsTabMargin;
	_commentsSort->setGeometry(sortX, row2Top, sortBtnWidth, st::topPostsTabHeight);
	sortX += sortBtnWidth + st::topPostsTabMargin;
	_viewsSort->setGeometry(sortX, row2Top, sortBtnWidth, st::topPostsTabHeight);

	auto filterX = halfWidth + 2 * st::topPostsTabMargin;
	_allFilter->setGeometry(filterX, row2Top, filterBtnWidth, st::topPostsTabHeight);
	filterX += filterBtnWidth + st::topPostsTabMargin;
	_docsFilter->setGeometry(filterX, row2Top, filterBtnWidth, st::topPostsTabHeight);
	filterX += filterBtnWidth + st::topPostsTabMargin;
	_mediaFilter->setGeometry(filterX, row2Top, filterBtnWidth, st::topPostsTabHeight);

	const auto baseHeight = st::topBarHeight + st::topPostsTabHeight + st::topPostsTabMargin;
	if (_bannerVisible) {
		_banner->setGeometry(0, baseHeight, newWidth, st::topPostsBannerHeight);
		_cancelButton->moveToRight(st::topPostsBannerPadding.right(), (st::topPostsBannerHeight - _cancelButton->height()) / 2);
		return baseHeight + st::topPostsBannerHeight;
	}
	return baseHeight;
}

void FixedBar::paintEvent(QPaintEvent *e) {
	Painter p(this);
	p.fillRect(e->rect(), st::topBarBg);

	p.setFont(st::topBarTitleFont);
	p.setPen(st::topBarTitleFg);
	const auto title = tr::ayu_TopPostsTitle(tr::now);
	const auto titleLeft = _backButton->width() + st::topPostsTabMargin;
	const auto titleY = (st::topBarHeight + p.fontMetrics().ascent()) / 2 - 2;
	p.drawText(titleLeft, titleY, title);
}

SectionWidget::SectionWidget(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer)
: Window::SectionWidget(parent, controller, rpl::single<PeerData*>(peer))
, _peer(peer)
, _fixedBar(this, controller, peer)
, _fixedBarShadow(this)
, _scroll(this, st::historyScroll, false) {
	_fixedBar->show();
	_fixedBarShadow->raise();
	_scroll->show();

	setupControls();
	loadPosts();
}

SectionWidget::~SectionWidget() = default;

void SectionWidget::setupControls() {
	_inner = _scroll->setOwnedWidget(
		object_ptr<InnerWidget>(this, controller(), _peer));

	_fixedBar->backRequests(
	) | rpl::on_next([=] {
		controller()->showBackFromStack();
	}, lifetime());

	_fixedBar->refreshRequests(
	) | rpl::on_next([=] {
		startSync();
	}, lifetime());

	_fixedBar->rescanRequests(
	) | rpl::on_next([=](int scanWindow) {
		startScan(scanWindow);
	}, lifetime());

	_fixedBar->clearCacheRequests(
	) | rpl::on_next([=] {
		clearCache();
	}, lifetime());

	_fixedBar->cancelRequests(
	) | rpl::on_next([=] {
		cancelScan();
		cancelSync();
	}, lifetime());

	_fixedBar->sortChanged(
	) | rpl::on_next([=](AyuDatabase::TopPostsSortOrder sortOrder) {
		_sortOrder = sortOrder;
		if (_inner) {
			_inner->setSortOrder(sortOrder);
		}
		loadPosts();
	}, lifetime());

	_fixedBar->filterChanged(
	) | rpl::on_next([=](AyuDatabase::TopPostsFilter filter) {
		_filter = filter;
		if (_inner) {
			_inner->setFilter(filter);
		}
		loadPosts();
	}, lifetime());

	_fixedBar->heightChanged(
	) | rpl::on_next([=](int newHeight) {
		updateLayout();
	}, lifetime());

	_scroll->scrolls(
	) | rpl::on_next([=] {
		onScroll();
	}, lifetime());

	_inner->clicks(
	) | rpl::on_next([=](MsgId msgId) {
		controller()->showPeerHistory(_peer, Window::SectionShow::Way::Forward, msgId);
	}, lifetime());

	AyuDatabase::channelPostRemoved(
	) | rpl::filter([=](const std::pair<AyuDatabase::ID, int> &pair) {
		return pair.first == _peer->id.bare;
	}) | rpl::on_next([=](const std::pair<AyuDatabase::ID, int> &pair) {
		loadPosts();
	}, lifetime());

	AyuDatabase::channelPostsCleared(
	) | rpl::filter([=](AyuDatabase::ID channelId) {
		return channelId == 0 || channelId == _peer->id.bare;
	}) | rpl::on_next([=] {
		loadPosts();
	}, lifetime());
}

not_null<PeerData*> SectionWidget::channel() const {
	return _peer;
}

Dialogs::RowDescriptor SectionWidget::activeChat() const {
	return {
		_peer->owner().history(_peer),
		FullMsgId(_peer->id, ShowAtUnreadMsgId)
	};
}

QPixmap SectionWidget::grabForShowAnimation(
		const Window::SectionSlideParams &params) {
	if (params.withTopBarShadow) {
		_fixedBarShadow->hide();
	}
	auto result = Ui::GrabWidget(this);
	if (params.withTopBarShadow) {
		_fixedBarShadow->show();
	}
	return result;
}

bool SectionWidget::showInternal(
		not_null<Window::SectionMemento*> memento,
		const Window::SectionShow &params) {
	if (const auto topMemento = dynamic_cast<SectionMemento*>(memento.get())) {
		if (topMemento->getPeer() == channel()) {
			setInternalState(geometry(), topMemento);
			return true;
		}
	}
	return false;
}

std::shared_ptr<Window::SectionMemento> SectionWidget::createMemento() {
	auto result = std::make_shared<SectionMemento>(_peer, _sortOrder, _filter);
	result->setScrollTop(_scroll->scrollTop());
	return result;
}

void SectionWidget::setInternalState(
		const QRect &geometry,
		not_null<SectionMemento*> memento) {
	setGeometry(geometry);
	Ui::SendPendingMoveResizeEvents(this);
	_sortOrder = memento->sortOrder();
	_filter = memento->filter();
	loadPosts();
	_scroll->scrollToY(memento->getScrollTop());
}

bool SectionWidget::floatPlayerHandleWheelEvent(QEvent *e) {
	return false;
}

QRect SectionWidget::floatPlayerAvailableRect() {
	return mapToGlobal(rect());
}

void SectionWidget::doSetInnerFocus() {
	if (_inner) {
		_inner->setFocus();
	}
}

void SectionWidget::showAnimatedHook(
		const Window::SectionSlideParams &params) {
}

void SectionWidget::showFinishedHook() {
	doSetInnerFocus();
}

void SectionWidget::paintEvent(QPaintEvent *e) {
	Painter p(this);
	p.fillRect(e->rect(), st::windowBg);
}

void SectionWidget::onScroll() {
	if (_inner) {
		const auto scrollTop = _scroll->scrollTop();
		_inner->setVisibleTopBottom(scrollTop, scrollTop + _scroll->height());
	}
}

void SectionWidget::updateLayout() {
	if (!width() || !height()) {
		return;
	}
	_fixedBar->resizeToWidth(width());
	_fixedBarShadow->setGeometry(0, _fixedBar->height(), width(), st::lineWidth);
	_scroll->setGeometry(
		0,
		_fixedBar->height(),
		width(),
		height() - _fixedBar->height());
}

void SectionWidget::resizeEvent(QResizeEvent *e) {
	updateLayout();
}

void SectionWidget::loadPosts() {
	const auto channelId = _peer->id.bare;
	auto posts = AyuDatabase::getChannelTopPosts(
		channelId,
		_sortOrder,
		_filter);

	if (posts.empty() && !AyuDatabase::hasChannelTopPosts(channelId) && !_initialScanAttempted) {
		_initialScanAttempted = true;
		if (_inner) {
			_inner->setEmptyMessage(tr::ayu_TopPostsEmptyInitial(tr::now));
			_inner->setPosts({});
		}
		startScan();
	} else {
		if (_inner) {
			if (posts.empty()) {
				switch (_filter) {
				case AyuDatabase::TopPostsFilter::Documents:
					_inner->setEmptyMessage(tr::ayu_TopPostsEmptyDocs(tr::now));
					break;
				case AyuDatabase::TopPostsFilter::Media:
					_inner->setEmptyMessage(tr::ayu_TopPostsEmptyMedia(tr::now));
					break;
				default:
					_inner->setEmptyMessage(tr::ayu_TopPostsEmptyMatching(tr::now));
					break;
				}
			}
			_inner->setPosts(std::move(posts));
		}
	}
}

void SectionWidget::startScan(int scanWindow) {
	if (_scanner) {
		return;
	}
	_fixedBar->showBanner(tr::ayu_TopPostsScanning(tr::now), true);
	_scanner = std::make_unique<Scanner>(_peer, scanWindow, _filter);
	_scanner->progress(
	) | rpl::on_next([=](const ScanProgress &p) {
		handleScanProgress(p);
	}, _scannerLifetime);
	_scanner->start();
}

void SectionWidget::cancelScan() {
	if (_scanner) {
		_scanner->cancel();
		_scanner = nullptr;
		_scannerLifetime.destroy();
		_fixedBar->hideBanner();
		loadPosts();
	}
}

void SectionWidget::handleScanProgress(const ScanProgress &p) {
	switch (p.state) {
	case ScanState::Scanning: {
		const auto text = tr::ayu_TopPostsScanningProgress(
			tr::now,
			lt_count,
			p.scannedCount,
			lt_total,
			p.targetCount);
		_fixedBar->showBanner(text, true);
		break;
	}
	case ScanState::PausedFloodWait: {
		const auto text = tr::ayu_TopPostsRateLimited(
			tr::now,
			lt_seconds,
			p.floodWaitSeconds);
		_fixedBar->showBanner(text, true);
		break;
	}
	case ScanState::Completed: {
		_fixedBar->hideBanner();
		_scanner = nullptr;
		_scannerLifetime.destroy();
		loadPosts();
		break;
	}
	case ScanState::Cancelled: {
		_fixedBar->hideBanner();
		_scanner = nullptr;
		_scannerLifetime.destroy();
		loadPosts();
		break;
	}
	case ScanState::Failed: {
		const auto text = p.errorMessage.isEmpty()
			? tr::ayu_TopPostsScanFailed(tr::now)
			: tr::ayu_TopPostsScanError(tr::now, lt_error, p.errorMessage);
		_fixedBar->showBanner(text, false);
		_scanner = nullptr;
		_scannerLifetime.destroy();
		break;
	}
	case ScanState::Idle:
		break;
	}
}

void SectionWidget::startSync() {
	if (_syncController) {
		return;
	}
	_fixedBar->showBanner(tr::ayu_TopPostsSyncing(tr::now), true);
	_syncController = std::make_unique<IncrementalSyncController>(_peer, _filter);
	_syncController->progress(
	) | rpl::on_next([=](const SyncProgress &p) {
		handleSyncProgress(p);
	}, _syncLifetime);
	_syncController->start();
}

void SectionWidget::cancelSync() {
	if (_syncController) {
		_syncController->cancel();
		_syncController = nullptr;
		_syncLifetime.destroy();
		_fixedBar->hideBanner();
		loadPosts();
	}
}

void SectionWidget::handleSyncProgress(const SyncProgress &p) {
	switch (p.state) {
	case SyncState::SyncingDeltas: {
		const auto text = tr::ayu_TopPostsSyncingDeltas(
			tr::now,
			lt_count,
			p.deltasFetched);
		_fixedBar->showBanner(text, true);
		break;
	}
	case SyncState::RefreshingCached: {
		const auto text = tr::ayu_TopPostsSyncingProgress(
			tr::now,
			lt_count,
			p.refreshedCount,
			lt_total,
			p.totalToRefresh);
		_fixedBar->showBanner(text, true);
		break;
	}
	case SyncState::PausedFloodWait: {
		const auto text = tr::ayu_TopPostsRateLimited(
			tr::now,
			lt_seconds,
			p.floodWaitSeconds);
		_fixedBar->showBanner(text, true);
		break;
	}
	case SyncState::Completed: {
		_fixedBar->hideBanner();
		_syncController = nullptr;
		_syncLifetime.destroy();
		loadPosts();
		break;
	}
	case SyncState::Cancelled: {
		_fixedBar->hideBanner();
		_syncController = nullptr;
		_syncLifetime.destroy();
		loadPosts();
		break;
	}
	case SyncState::Failed: {
		const auto text = p.errorMessage.isEmpty()
			? tr::ayu_TopPostsSyncFailed(tr::now)
			: tr::ayu_TopPostsSyncError(tr::now, lt_error, p.errorMessage);
		_fixedBar->showBanner(text, false);
		_syncController = nullptr;
		_syncLifetime.destroy();
		break;
	}
	case SyncState::Idle:
		break;
	}
}

void SectionWidget::clearCache() {
	AyuDatabase::clearChannelPosts(_peer->id.bare);
	if (_inner) {
		_inner->setEmptyMessage(tr::ayu_TopPostsCacheCleared(tr::now));
		_inner->setPosts({});
	}
}

SectionMemento::SectionMemento(
	not_null<PeerData*> peer,
	AyuDatabase::TopPostsSortOrder sortOrder,
	AyuDatabase::TopPostsFilter filter)
: _peer(peer)
, _sortOrder(sortOrder)
, _filter(filter) {
}

object_ptr<Window::SectionWidget> SectionMemento::createWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		Window::Column column,
		const QRect &geometry) {
	if (column == Window::Column::Third) {
		return nullptr;
	}
	auto result = object_ptr<SectionWidget>(parent, controller, _peer);
	result->setInternalState(geometry, this);
	return result;
}

} // namespace TopPosts
