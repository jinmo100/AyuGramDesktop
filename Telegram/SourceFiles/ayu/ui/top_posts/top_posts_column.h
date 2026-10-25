// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "base/basic_types.h"
#include "base/object_ptr.h"
#include "ui/effects/animations.h"
#include "ui/rp_widget.h"

class QKeyEvent;
class PeerData;

namespace Window {
class SessionController;
class SectionWidget;
} // namespace Window

namespace TopPosts {

// Kept alive between showings, so the ranking state survives post visits.
class Column final : public Ui::RpWidget {
public:
	Column(
		QWidget *parent,
		not_null<Window::SessionController*> controller);
	~Column();

	void showChannel(not_null<PeerData*> channel);
	void hideColumn();
	void setInnerFocus();
	[[nodiscard]] bool shown() const {
		return _shown;
	}

	void setClosedCallback(Fn<void()> callback) {
		_closed = std::move(callback);
	}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void keyPressEvent(QKeyEvent *e) override;

private:
	void createWidget(not_null<PeerData*> channel);
	void updateSlide();

	const not_null<Window::SessionController*> _controller;
	object_ptr<Window::SectionWidget> _widget = { nullptr };
	PeerData *_channel = nullptr;
	Fn<void()> _closed;
	Ui::Animations::Simple _slide;
	bool _shown = false;

};

void ShowTopPosts(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> channel);

} // namespace TopPosts
