#include "logging.hpp"
#include "native_api.hpp"
#include <cstdio>
#include <cstring>
#include <jni.h>
#include <string>

// Global variable to store the hook function pointer provided by LSPosed.
// We receive this in `native_init` and can then use it anywhere else in our
// code.
static HookFunType hook_func = nullptr;

/*
 * =========================================================================================
 *  Example 1: A simple function hook
 * =========================================================================================
 *
 * This demonstrates the most basic form of a hook. We have
 * a target function (which we assume is named `target_fun` in `libtarget.so`),
 * a replacement function (`fake`), and
 * a backup function pointer (`backup`) to call the original.
 *
 * ASCII Art: Hooking Mechanism
 *
 *
 *    Your App's Code
 *   ------------------
 *           |
 *     Calls `target_fun()`
 *           |
 *           |
 *           | (The hook redirects the call)
 *           |
 *           +---------------> Your `fake()` function
 *                            ----------------------
 *                                    |
 *                              (Optional) Calls `backup()`
 *                                    |
 *           +-------------- (This executes the original `target_fun` code)
 *           |
 *       Returns modified value
 *
 */

// A backup pointer to store the original `target_fun`.
// It's crucial that this has the same signature as the original function.
int (*backup)();

// Our replacement function. It must have the same signature as the target.
int fake() {
  // We call the original function via our `backup` pointer
  // and modify its result.
  return backup() + 1;
}

/*
 * =========================================================================================
 *  Example 2: Hooking a standard library function (fopen)
 * =========================================================================================
 *
 * This shows how to hook a common C library function. This hook will apply to
 * the entire process. Here, we intercept calls to `fopen` to prevent files with
 * "banned" in their name from being opened.
 */

// Backup pointer for the original `fopen`.
FILE *(*backup_fopen)(const char *filename, const char *mode);

// Our replacement `fopen` function.
FILE *fake_fopen(const char *filename, const char *mode) {
  // Check if the filename contains the substring "banned".
  if (strstr(filename, "banned")) {
    // If it does, we deny the request by returning nullptr.
    return nullptr;
  }
  // Otherwise, we call the original `fopen` and let it proceed as normal.
  return backup_fopen(filename, mode);
}

/*
 * =========================================================================================
 *  Example 3: Hooking a JNI function (FindClass)
 * =========================================================================================
 *
 * This demonstrates hooking a function from the JNI interface table.
 * This is powerful for manipulating the behavior of Java code from native.
 * Here, we prevent the class `dalvik.system.BaseDexClassLoader` from being
 * loaded.
 */

// Backup pointer for the original JNI `FindClass`.
jclass (*backup_FindClass)(JNIEnv *env, const char *name);

// Our replacement `FindClass` function.
jclass fake_FindClass(JNIEnv *env, const char *name) {
  // We check for a specific class name.
  if (!strcmp(name, "dalvik/system/BaseDexClassLoader")) {
    // And block it from being found.
    return nullptr;
  }
  // For all other classes, we call the original function.
  return backup_FindClass(env, name);
}

/**
 * @brief The "OnModuleLoaded" callback.
 *
 * This function is returned by `native_init` and is called by LSPosed
 * every time a library is loaded in the target process.
 * This is ideal for "targeted" hooks that should only be applied
 * when a specific library is present.
 *
 * @param name The name of the loaded library (e.g., "libtarget.so").
 * @param handle A handle to the library for use with `dlsym`.
 */
void on_library_loaded(const char *name, void *handle) {
  // We check if the name of the loaded library ends with "libtarget.so".
  if (std::string(name).ends_with("libtarget.so")) {
    // If it does, we can now safely look for symbols within it.
    void *target = dlsym(handle, "target_fun");
    // And apply our hook.
    hook_func(target, (void *)fake, (void **)&backup);
  }
}

/**
 * @brief Standard JNI entry point.
 *
 * This function is called by the Android Runtime
 * when the Java side executes `System.loadLibrary()`.
 *
 * It is called *after* `native_init`.
 * By this time, `hook_func` has already been initialized,
 * so we can use it to set up JNI-related hooks.
 */
extern "C" [[gnu::visibility("default")]] [[gnu::used]]
jint JNI_OnLoad(JavaVM *jvm, void *) {
  LOGD("JNI_OnLoad called");
  JNIEnv *env = nullptr;
  jvm->GetEnv((void **)&env, JNI_VERSION_1_6);

  // Here, we hook the `FindClass` function from the JNI function table.
  // `env->functions` points to a table of JNI function pointers.
  hook_func((void *)env->functions->FindClass, (void *)fake_FindClass,
            (void **)&backup_FindClass);

  return JNI_VERSION_1_6;
}

/**
 * @brief The main LSPosed native entry point.
 *
 * This is the FIRST function called in your library by LSPosed. It is the
 * core of the integration. Its purpose is to receive the API function pointers
 * from LSPosed and to return a callback for future library load events.
 *
 * ASCII Art: Execution Order
 *
 *  System.loadLibrary("demo")
 *           |
 *           v
 *    linker `dlopen`
 *           |
 *           v
 * [LSPosed's hook on `dlopen` executes]
 *           |
 *           v
 *  `native_init()` is called <-- YOU ARE HERE (FIRST)
 *           |
 *           v
 *  Android Runtime calls `JNI_OnLoad()` <-- YOU ARE HERE (SECOND)
 *
 *
 * @param entries A pointer to the struct containing the hook/unhook functions.
 * @return A function pointer to our `on_library_loaded` callback.
 */
extern "C" [[gnu::visibility("default")]] [[gnu::used]]
NativeOnModuleLoaded native_init(const NativeAPIEntries *entries) {
  LOGD("NativeOnModuleLoaded called");
  // 1. Save the hook function pointer from the `entries` struct
  //    into our global variable.
  hook_func = entries->hookFunc;

  // 2. Perform any "global" or "early" hooks that should be active immediately.
  //    Here, we hook `fopen` from the C standard library.
  hook_func((void *)fopen, (void *)fake_fopen, (void **)&backup_fopen);

  // 3. Return the function pointer to our callback.
  // LSPosed will now call `on_library_loaded` whenever a new library is loaded.
  return on_library_loaded;
}
