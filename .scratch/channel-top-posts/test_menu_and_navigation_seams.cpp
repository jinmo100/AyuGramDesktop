#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <functional>
#include <memory>
#include <algorithm>

namespace MenuAndNavSeams {

using ID = long long;

enum class PeerType {
	User,
	Chat,
	BroadcastChannel,
	Megagroup,
};

struct MockPeer {
	ID id = 0;
	PeerType type = PeerType::BroadcastChannel;

	[[nodiscard]] bool isChannel() const {
		return type == PeerType::BroadcastChannel || type == PeerType::Megagroup;
	}
	[[nodiscard]] bool isMegagroup() const {
		return type == PeerType::Megagroup;
	}
	[[nodiscard]] bool isUser() const {
		return type == PeerType::User;
	}
	[[nodiscard]] bool isChat() const {
		return type == PeerType::Chat;
	}
};

// Seam 1: Determines if Top Posts action should be added to the context menu
bool ShouldShowTopPostsAction(const MockPeer *peer) {
	if (!peer || !peer->isChannel() || peer->isMegagroup()) {
		return false;
	}
	return true;
}

// Seam 2: Simulates timeline navigation on card click
struct NavRecord {
	ID peerId = 0;
	int msgId = 0;
	bool highlighted = false;
};

class MockSessionNavigation {
public:
	void historyView_showAt(const MockPeer *peer, int msgId) {
		assert(peer != nullptr);
		_lastNav = NavRecord{
			.peerId = peer->id,
			.msgId = msgId,
			.highlighted = true,
		};
	}

	[[nodiscard]] const NavRecord &lastNav() const {
		return _lastNav;
	}

private:
	NavRecord _lastNav;
};

// Seam 3: Simulates deletion hook & view synchronization
struct MockTopPost {
	ID channelId = 0;
	int messageId = 0;
	int reactionsCount = 0;
};

class MockDatabase {
public:
	std::vector<MockTopPost> posts;
	std::vector<std::function<void(ID, int)>> postRemovedObservers;
	std::vector<std::function<void(ID)>> postsClearedObservers;

	void addPost(ID channelId, int msgId, int reactions) {
		posts.push_back({ channelId, msgId, reactions });
	}

	void removePost(ID channelId, int messageId) {
		posts.erase(
			std::remove_if(posts.begin(), posts.end(), [&](const MockTopPost &p) {
				return p.channelId == channelId && p.messageId == messageId;
			}),
			posts.end());
		for (const auto &cb : postRemovedObservers) {
			cb(channelId, messageId);
		}
	}

	void clearAllChannelPosts() {
		posts.clear();
		for (const auto &cb : postsClearedObservers) {
			cb(0);
		}
	}

	[[nodiscard]] size_t count(ID channelId) const {
		size_t c = 0;
		for (const auto &p : posts) {
			if (p.channelId == channelId) ++c;
		}
		return c;
	}
};

// Seam 4: Simulates section widget observing deletion
class MockSectionWidget {
public:
	MockSectionWidget(ID channelId, MockDatabase &db)
		: _channelId(channelId), _db(db) {
		reload();
		_db.postRemovedObservers.push_back([this](ID cid, int mid) {
			if (cid == _channelId) {
				reload();
			}
		});
		_db.postsClearedObservers.push_back([this](ID cid) {
			if (cid == 0 || cid == _channelId) {
				reload();
			}
		});
	}

	void reload() {
		_currentPosts.clear();
		for (const auto &p : _db.posts) {
			if (p.channelId == _channelId) {
				_currentPosts.push_back(p);
			}
		}
	}

	[[nodiscard]] const std::vector<MockTopPost> &currentPosts() const {
		return _currentPosts;
	}

private:
	ID _channelId = 0;
	MockDatabase &_db;
	std::vector<MockTopPost> _currentPosts;
};

void TestMenuVisibility() {
	MockPeer broadcast{ .id = 1001, .type = PeerType::BroadcastChannel };
	MockPeer megagroup{ .id = 1002, .type = PeerType::Megagroup };
	MockPeer chat{ .id = 1003, .type = PeerType::Chat };
	MockPeer user{ .id = 1004, .type = PeerType::User };

	assert(ShouldShowTopPostsAction(&broadcast) == true);
	assert(ShouldShowTopPostsAction(&megagroup) == false);
	assert(ShouldShowTopPostsAction(&chat) == false);
	assert(ShouldShowTopPostsAction(&user) == false);
	assert(ShouldShowTopPostsAction(nullptr) == false);
	std::cout << "Test 1 (Menu visibility) passed.\n";
}

void TestTimelineNavigation() {
	MockPeer broadcast{ .id = 2001, .type = PeerType::BroadcastChannel };
	MockSessionNavigation nav;

	// User clicks on card with message ID 42
	nav.historyView_showAt(&broadcast, 42);

	const auto &last = nav.lastNav();
	assert(last.peerId == 2001);
	assert(last.msgId == 42);
	assert(last.highlighted == true);
	std::cout << "Test 2 (Timeline navigation jump & highlight) passed.\n";
}

void TestRevocationSynchronization() {
	MockDatabase db;
	db.addPost(3001, 10, 50);
	db.addPost(3001, 11, 40);
	db.addPost(3001, 12, 30);
	db.addPost(3002, 20, 100);

	MockSectionWidget widget(3001, db);
	assert(widget.currentPosts().size() == 3);

	// Incoming message revocation update for channel 3001, msgId 11
	std::vector<int> revokedIds = { 11 };
	for (int id : revokedIds) {
		db.removePost(3001, id);
	}

	// Post 11 should be gone from DB
	assert(db.count(3001) == 2);
	assert(db.count(3002) == 1);

	// Widget view should have automatically updated and post 11 disappeared
	assert(widget.currentPosts().size() == 2);
	assert(widget.currentPosts()[0].messageId == 10);
	assert(widget.currentPosts()[1].messageId == 12);
	std::cout << "Test 3 (Revocation hook & auto-removal from view) passed.\n";
}

void TestClearAllTopPostsCache() {
	MockDatabase db;
	db.addPost(4001, 1, 10);
	db.addPost(4001, 2, 20);
	db.addPost(4002, 1, 30);


	MockSectionWidget widget(4001, db);
	assert(widget.currentPosts().size() == 2);
	assert(db.posts.size() == 3);

	// Global settings "Clear All Top Posts Caches" action
	db.clearAllChannelPosts();
	assert(db.posts.empty());
	assert(widget.currentPosts().empty());
	std::cout << "Test 4 (Clear all caches in settings) passed.\n";
}

} // namespace MenuAndNavSeams

int main() {
	MenuAndNavSeams::TestMenuVisibility();
	MenuAndNavSeams::TestTimelineNavigation();
	MenuAndNavSeams::TestRevocationSynchronization();
	MenuAndNavSeams::TestClearAllTopPostsCache();
	std::cout << "All menu and navigation seam tests passed successfully!\n";
	return 0;
}
