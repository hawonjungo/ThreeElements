#pragma once
#ifndef SOCIAL_H_
#define SOCIAL_H_

// Rate, share and feedback (1.9.16, after the closed-test report): the links out of the game. Presentation only,
// like Online and WebBoards; every call is safe on every platform.
//   Android: Google Play's in-app review card, the Play page, the system share sheet (ThreeElementsActivity).
//   Web:     the browser's share sheet (or the clipboard), a new tab for the support page.
//   PC:      the clipboard, the default browser.
namespace social
{
	enum class ShareResult { Opened, Copied, Failed };  // Copied: the text is on the clipboard (no share sheet here)

	bool CanRate();                          // a Play page to rate the game on: Android only
	void OpenStorePage();                    // RATE in SETTINGS: the game's page in the Play Store
	void AskReview();                        // Google's in-app review card; Google decides whether it shows
	ShareResult Share(const char* text);     // the text holds the links
	void OpenFeedback();                     // FEEDBACK: the support page (email and FAQ)
}

#endif // SOCIAL_H_
