#include "Online.h"

#ifdef __ANDROID__

#include "Define.h"
#include <jni.h>

// The Java side is ThreeElementsActivity (android/app/src/main/java): four small methods that hand over to Play
// Games. They are looked up on the activity object itself, which works from SDL's native thread (FindClass would
// not find application classes there).
namespace
{
	struct Call
	{
		JNIEnv* env;
		jobject activity;
		jclass cls;
		jmethodID method;

		Call(const char* name, const char* signature) : env(NULL), activity(NULL), cls(NULL), method(NULL)
		{
			env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
			activity = static_cast<jobject>(SDL_AndroidGetActivity());
			if (env == NULL || activity == NULL)
				return;
			cls = env->GetObjectClass(activity);
			method = env->GetMethodID(cls, name, signature);
			if (method == NULL)
				env->ExceptionClear();  // an activity without the method: the feature is simply off
		}
		~Call()
		{
			if (env == NULL)
				return;
			if (env->ExceptionCheck())
				env->ExceptionClear();
			if (cls != NULL)
				env->DeleteLocalRef(cls);
			if (activity != NULL)
				env->DeleteLocalRef(activity);
		}
		bool Ready() const { return method != NULL; }
	};
}

namespace online
{
	bool Available()
	{
		static int known = -1;  // -1 = not asked yet
		if (known < 0)
		{
			Call call("onlineAvailable", "()Z");
			known = call.Ready() && call.env->CallBooleanMethod(call.activity, call.method) ? 1 : 0;
		}
		return known == 1;
	}

	bool SignedIn()
	{
		if (!Available())
			return false;
		Call call("onlineSignedIn", "()Z");
		return call.Ready() && call.env->CallBooleanMethod(call.activity, call.method);
	}

	void Submit(Board board, long long score)
	{
		if (!Available() || score <= 0)
			return;
		Call call("onlineSubmit", "(IJ)V");
		if (call.Ready())
			call.env->CallVoidMethod(call.activity, call.method, static_cast<jint>(board == Board::Play ? 0 : 1),
				static_cast<jlong>(score));
	}

	void ShowBoards()
	{
		if (!Available())
			return;
		Call call("onlineShow", "()V");
		if (call.Ready())
			call.env->CallVoidMethod(call.activity, call.method);
	}
}

#else  // PC and web: no online boards

namespace online
{
	bool Available() { return false; }
	bool SignedIn() { return false; }
	void Submit(Board, long long) {}
	void ShowBoards() {}
}

#endif
