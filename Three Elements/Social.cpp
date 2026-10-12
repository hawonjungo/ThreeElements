#include "Social.h"
#include "Define.h"

namespace
{
	const char* const SUPPORT_URL = "https://injoker.relifes.net/support/";
}

#ifdef __ANDROID__

#include <jni.h>

// The Java side is ThreeElementsActivity: socialRate / socialReview / socialShare, looked up on the activity object
// (like Online.cpp does), so an activity without them simply turns the feature off.
namespace
{
	bool CallActivity(const char* name, const char* signature, const char* text)
	{
		JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
		jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
		if (env == NULL || activity == NULL)
			return false;
		jclass cls = env->GetObjectClass(activity);
		jmethodID method = env->GetMethodID(cls, name, signature);
		bool done = false;
		if (method != NULL)
		{
			if (text != NULL)
			{
				jstring s = env->NewStringUTF(text);
				env->CallVoidMethod(activity, method, s);
				env->DeleteLocalRef(s);
			}
			else
				env->CallVoidMethod(activity, method);
			done = true;
		}
		if (env->ExceptionCheck())
		{
			env->ExceptionClear();
			done = false;
		}
		env->DeleteLocalRef(cls);
		env->DeleteLocalRef(activity);
		return done;
	}
}

namespace social
{
	bool CanRate() { return true; }
	void OpenStorePage() { CallActivity("socialRate", "()V", NULL); }
	void AskReview() { CallActivity("socialReview", "()V", NULL); }
	ShareResult Share(const char* text)
	{
		return CallActivity("socialShare", "(Ljava/lang/String;)V", text) ? ShareResult::Opened : ShareResult::Failed;
	}
	void OpenFeedback() { SDL_OpenURL(SUPPORT_URL); }
}

#elif defined(__EMSCRIPTEN__)

#include <emscripten.h>

// navigator.share where the browser has it (phones), else the clipboard. Called from the frame after the tap or key,
// still inside the browser's "user activation" window, which both of them need.
EM_JS(int, js_social_share, (const char* text), {
	var t = UTF8ToString(text);
	try {
		if (navigator.share) {
			navigator.share({ text: t }).catch(function (e) {
				// refused (not cancelled by the player): the clipboard instead
				if (e && e.name !== 'AbortError' && navigator.clipboard)
					navigator.clipboard.writeText(t).catch(function () {});
			});
			return 0;
		}
		if (navigator.clipboard && navigator.clipboard.writeText) {
			navigator.clipboard.writeText(t).catch(function () {});
			return 1;
		}
	} catch (e) {}
	return 2;
});

EM_JS(void, js_social_open, (const char* url), {
	try { window.open(UTF8ToString(url), '_blank', 'noopener'); } catch (e) {}
});

namespace social
{
	bool CanRate() { return false; }
	void OpenStorePage() {}
	void AskReview() {}
	ShareResult Share(const char* text)
	{
		int r = js_social_share(text);
		return r == 0 ? ShareResult::Opened : r == 1 ? ShareResult::Copied : ShareResult::Failed;
	}
	void OpenFeedback() { js_social_open(SUPPORT_URL); }
}

#else  // PC

namespace social
{
	bool CanRate() { return false; }
	void OpenStorePage() {}
	void AskReview() {}
	ShareResult Share(const char* text)
	{
		return SDL_SetClipboardText(text) == 0 ? ShareResult::Copied : ShareResult::Failed;
	}
	void OpenFeedback() { SDL_OpenURL(SUPPORT_URL); }
}

#endif
