#pragma once

#include <QUrl>

// Pure navigation rules (FEATURES W1): what stays in the app and what goes to
// the desktop. The built-in browser is general purpose, so every web page
// stays in it; the owner signed in to SoundCloud and every link off the
// sign-in page left for the system browser under the old YouTube-only rule.
namespace pldl::core {

/// True only for the few non-web schemes the desktop handles (mailto, tel,
/// magnet). http and https never leave the app.
[[nodiscard]] bool shouldOpenExternally(const QUrl& url);

/// True for pop-ups that stay in-app: any web page (sign-in windows included).
[[nodiscard]] bool isInAppPopupUrl(const QUrl& url);

/// Rewrites YouTube's redirect wrapper (youtube.com/redirect?q=...) to its target.
[[nodiscard]] QUrl unwrapRedirect(const QUrl& url);

} // namespace pldl::core
