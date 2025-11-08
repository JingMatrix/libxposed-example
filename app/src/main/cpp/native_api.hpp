#pragma once

#include <cstdint>
#include <dlfcn.h>

/*
 * =========================================================================================
 *  LSPosed Native API Interface
 * =========================================================================================
 *
 * This header defines the essential functions and data structures that allow a
 * native library (your module) to interface with the LSPosed framework. The
 * core idea is that LSPosed provides a set of powerful tools (like function
 * hooking), and your module consumes these tools through a well-defined entry
 * point.
 *
 * The interaction flow is as follows:
 *
 *   1. LSPosed intercepts the loading of your native library (e.g., libnative.so).
 *   2. LSPosed looks for and calls the `native_init` function within your library.
 *   3. LSPosed passes a `NativeAPIEntries` struct to your `native_init`,
 *      which contains function pointers to LSPosed's hooking
 *      and unhooking implementations (powered by Dobby).
 *   4. Your `native_init` function saves these function pointers for later use
 *      and returns a callback function (`NativeOnModuleLoaded`).
 *   5. LSPosed will then invoke your returned callback every time
 *      a new native library is loaded into the target process,
 *      allowing you to perform "late" hooks on specific libraries.
 *
 *
 * ASCII Art: Initialization Flow
 *
 *   LSPosed Framework                    Your Native Module (e.g., libnative.so)
 *   -----------------                    -------------------------------------
 *
 *        |                                            |
 * [ Intercepts dlopen("libnative.so") ]               |
 *        |                                            |
 *        |----------> [ Finds & Calls native_init() ] |
 *        |                                            |
 *   [ Passes NativeAPIEntries* ]  ---> [ Stores function pointers ]
 *   (Contains hook/unhook funcs)                      |
 *        |                                            |
 *        |                                            |
 *        |             <-----------[ Returns `NativeOnModuleLoaded` callback ]
 *        |                                            |
 *        |                                            |
 *   [ Stores your callback ]                          |
 *        |                                            |
 *
 */

/**
 * @brief Defines the function signature for a hooking function.
 *
 * This is a pointer to the function provided by LSPosed that you will use to
 * install a hook.
 *
 * @param func A pointer to the target function you want to hook.
 * @param replace A pointer to your new, replacement function.
 * @param backup A pointer to a variable where the address of the original,
 *               un-hooked function will be stored. You use this to call the
 *               original implementation from your replacement function.
 * @return 0 on success.
 */
typedef int (*HookFunType)(void *func, void *replace, void **backup);

/**
 * @brief Defines the function signature for an unhooking function.
 *
 * This is a pointer to the function provided by LSPosed to remove a hook
 * that you previously installed.
 *
 * @param func A pointer to the target function that was hooked.
 * @return 0 on success.
 */
typedef int (*UnhookFunType)(void *func);

/**
 * @brief Defines the function signature for the "module loaded" callback.
 *
 * You implement a function with this signature and return it from `native_init`.
 * LSPosed will call your function every time a native library
 * is loaded into the target process.
 *
 * @param name The file name or path of the loaded library (e.g., "libart.so").
 * @param handle A handle to the loaded library, which can be used with `dlsym`.
 */
typedef void (*NativeOnModuleLoaded)(const char *name, void *handle);

/**
 * @brief A struct containing the function pointers provided by LSPosed.
 *
 * LSPosed passes a pointer to this struct to your `native_init` function.
 * It's the bridge that gives you access to LSPosed's native capabilities.
 */
typedef struct {
  uint32_t version;         // The version of the native API.
  HookFunType hookFunc;     // Pointer to the hooking function.
  UnhookFunType unhookFunc; // Pointer to the unhooking function.
} NativeAPIEntries;

/**
 * @brief Defines the signature for the main entry point of your native module.
 *
 * You MUST export a function named `native_init` with this signature.
 * It's the very first function called by LSPosed in your library,
 * even before JNI_OnLoad.
 *
 * @param entries A pointer to the `NativeAPIEntries` struct provided by LSPosed.
 * @return A pointer to your `NativeOnModuleLoaded` callback function.
 *         If you don't need to be notified of library loads, you can return `nullptr`.
 */
typedef NativeOnModuleLoaded (*NativeInit)(const NativeAPIEntries *entries);
