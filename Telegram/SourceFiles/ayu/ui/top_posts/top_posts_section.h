// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/data/ayu_database.h"
#include "ayu/data/entities.h"
#include "ayu/features/top_posts/top_posts_scanner.h"
#include "ayu/features/top_posts/top_posts_sync.h"
#include "base/object_ptr.h"
#include "window/section_memento.h"
#include "window/section_widget.h"

namespace Ui {
class ScrollArea;
class PlainShadow;
class FlatButton;
class IconButton;
class PopupMenu;
} // namespace Ui

namespace Profile {
class BackButton;
} // namespace Profile

namespace TopPosts {

class FixedBar;
class InnerWidget;
class SectionMemento;

class SectionWidget final : public Window::SectionWidget {
public:
	SectionWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer);
	~SectionWidget() override;

	[[nodiscard]] not_null<PeerData*> channel() const;
	[[nodiscard]] Dialogs::RowDescriptor activeChat() const override;

	[[nodiscard]] bool hasTopBarShadow() const override {
		return true;
	}

	[[nodiscard]] QPixmap grabForShowAnimation(
		const Window::SectionSlideParams &params) override;

	bool showInternal(
		not_null<Window::SectionMemento*> memento,
		const Window::SectionShow &params) override;
	std::shared_ptr<Window::SectionMemento> createMemento() override;

	void setInternalState(
		const QRect &geometry,
		not_null<SectionMemento*> memento);

	bool floatPlayerHandleWheelEvent(QEvent *e) override;
	QRect floatPlayerAvailableRect() override;

protected:
	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void showAnimatedHook(
		const Window::SectionSlideParams &params) override;
	void showFinishedHook() override;
	void doSetInnerFocus() override;

private:
	void setupControls();
	void loadPosts();
	void startScan(int scanWindow = kDefaultScanWindow);
	void cancelScan();
	void startSync();
	void cancelSync();
	void clearCache();
	void handleScanProgress(const ScanProgress &p);
	void handleSyncProgress(const SyncProgress &p);
	void onScroll();
	void updateLayout();

	const not_null<PeerData*> _peer;
	AyuDatabase::TopPostsSortOrder _sortOrder = AyuDatabase::TopPostsSortOrder::Reactions;
	AyuDatabase::TopPostsFilter _filter = AyuDatabase::TopPostsFilter::All;
	bool _initialScanAttempted = false;

	object_ptr<FixedBar> _fixedBar;
	object_ptr<Ui::PlainShadow> _fixedBarShadow;
	object_ptr<Ui::ScrollArea> _scroll;
	QPointer<InnerWidget> _inner;

	std::unique_ptr<Scanner> _scanner;
	rpl::lifetime _scannerLifetime;

	std::unique_ptr<IncrementalSyncController> _syncController;
	rpl::lifetime _syncLifetime;

};

class SectionMemento : public Window::SectionMemento {
public:
	explicit SectionMemento(
		not_null<PeerData*> peer,
		AyuDatabase::TopPostsSortOrder sortOrder = AyuDatabase::TopPostsSortOrder::Reactions,
		AyuDatabase::TopPostsFilter filter = AyuDatabase::TopPostsFilter::All);

	object_ptr<Window::SectionWidget> createWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		Window::Column column,
		const QRect &geometry) override;

	[[nodiscard]] not_null<PeerData*> getPeer() const {
		return _peer;
	}

	void setScrollTop(int scrollTop) {
		_scrollTop = scrollTop;
	}
	[[nodiscard]] int getScrollTop() const {
		return _scrollTop;
	}

	[[nodiscard]] AyuDatabase::TopPostsSortOrder sortOrder() const {
		return _sortOrder;
	}
	[[nodiscard]] AyuDatabase::TopPostsFilter filter() const {
		return _filter;
	}

private:
	const not_null<PeerData*> _peer;
	int _scrollTop = 0;
	AyuDatabase::TopPostsSortOrder _sortOrder = AyuDatabase::TopPostsSortOrder::Reactions;
	AyuDatabase::TopPostsFilter _filter = AyuDatabase::TopPostsFilter::All;

};

} // namespace TopPosts
