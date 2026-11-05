#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <vulkan/vulkan.h>

static VkSwapchainKHR probe_swapchain;
static Display *probe_display;
static Window probe_window;
static _Atomic uint32_t probe_image_count;
static _Atomic uint32_t probe_acquisitions;
static _Atomic uint32_t probe_presents;
static _Atomic int probe_nonsequential;
static _Atomic int probe_failure;
static _Atomic int close_event_sent;

static int inject_once(const char *name, _Atomic int *state) {
    return getenv(name) != NULL && !atomic_exchange(state, 1);
}

static _Atomic int acquire_injected;
static _Atomic int present_injected;

/* Reverse the enumerated handles and relabel acquire/present indices consistently.
 * The engine then sees valid swapchain ownership with a non-round-robin index
 * sequence, exercising its real image-acquisition path. */

static void remember_swapchain(VkSwapchainKHR swapchain) {
    if (probe_swapchain == VK_NULL_HANDLE) {
        probe_swapchain = swapchain;
    }
    assert(probe_swapchain == swapchain && "probe supports one swapchain");
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(
    VkDevice device, VkSwapchainKHR swapchain, uint32_t *image_count,
    VkImage *images) {
    typedef VkResult(VKAPI_PTR *Function)(VkDevice, VkSwapchainKHR, uint32_t *, VkImage *);
    static Function real_function;
    if (real_function == NULL) {
        real_function = (Function)dlsym(RTLD_NEXT, "vkGetSwapchainImagesKHR");
    }
    if (real_function == NULL) {
        atomic_store(&probe_failure, 1);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    remember_swapchain(swapchain);
    VkResult result = real_function(device, swapchain, image_count, images);
    if (result == VK_SUCCESS && image_count != NULL) {
        atomic_store(&probe_image_count, *image_count);
        if (images != NULL && *image_count > 0) {
            for (uint32_t left = 0, right = *image_count - 1; left < right; ++left, --right) {
                VkImage image = images[left];
                images[left] = images[right];
                images[right] = image;
            }
        }
    }
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(
    VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
    VkSemaphore semaphore, VkFence fence, uint32_t *image_index) {
    typedef VkResult(VKAPI_PTR *Function)(VkDevice, VkSwapchainKHR, uint64_t,
                                         VkSemaphore, VkFence, uint32_t *);
    static Function real_function;
    if (real_function == NULL) {
        real_function = (Function)dlsym(RTLD_NEXT, "vkAcquireNextImageKHR");
    }
    if (real_function == NULL) {
        atomic_store(&probe_failure, 1);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    remember_swapchain(swapchain);
    assert(image_index != NULL);
    uint32_t native_index = 0;
    VkResult result = real_function(device, swapchain, timeout, semaphore, fence,
                                    &native_index);
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        uint32_t count = atomic_load(&probe_image_count);
        assert(count > 0 && native_index < count);
        uint32_t relabeled_index = count - 1 - native_index;
        uint32_t ordinal = atomic_fetch_add(&probe_acquisitions, 1);
        if (relabeled_index != ordinal % count) {
            atomic_store(&probe_nonsequential, 1);
        }
        *image_index = relabeled_index;
        if (result == VK_SUCCESS &&
            inject_once("SWAPCHAIN_PROBE_SUBOPTIMAL_ACQUIRE", &acquire_injected)) {
            result = VK_SUBOPTIMAL_KHR;
        }
    }
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,
                                                 const VkPresentInfoKHR *info) {
    typedef VkResult(VKAPI_PTR *Function)(VkQueue, const VkPresentInfoKHR *);
    static Function real_function;
    if (real_function == NULL) {
        real_function = (Function)dlsym(RTLD_NEXT, "vkQueuePresentKHR");
    }
    if (real_function == NULL) {
        atomic_store(&probe_failure, 1);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    assert(info != NULL && info->swapchainCount == 1 &&
           "probe supports one swapchain per present");
    remember_swapchain(info->pSwapchains[0]);
    uint32_t count = atomic_load(&probe_image_count);
    assert(count > 0 && info->pImageIndices[0] < count);

    uint32_t native_index = count - 1 - info->pImageIndices[0];
    VkPresentInfoKHR native_info = *info;
    native_info.pImageIndices = &native_index;
    VkResult result = real_function(queue, &native_info);
    VkResult per_swapchain_result =
        info->pResults == NULL ? result : info->pResults[0];
    if ((result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) &&
        (per_swapchain_result == VK_SUCCESS ||
         per_swapchain_result == VK_SUBOPTIMAL_KHR)) {
        atomic_fetch_add(&probe_presents, 1);
        if (result == VK_SUCCESS &&
            inject_once("SWAPCHAIN_PROBE_SUBOPTIMAL_PRESENT", &present_injected)) {
            result = VK_SUBOPTIMAL_KHR;
        }
    }
    return result;
}

int XMapWindow(Display *display, Window window) {
    typedef int (*Function)(Display *, Window);
    static Function real_function;
    if (real_function == NULL) {
        real_function = (Function)dlsym(RTLD_NEXT, "XMapWindow");
    }
    if (real_function == NULL) {
        atomic_store(&probe_failure, 1);
        return 0;
    }
    int result = real_function(display, window);
    if (probe_window == None) {
        probe_display = display;
        probe_window = window;
    }
    return result;
}

int XPending(Display *display) {
    typedef int (*Function)(Display *);
    static Function real_function;
    if (real_function == NULL) {
        real_function = (Function)dlsym(RTLD_NEXT, "XPending");
    }
    if (real_function == NULL) {
        atomic_store(&probe_failure, 1);
        return 0;
    }

    uint32_t count = atomic_load(&probe_image_count);
    uint32_t present_limit = count > 4 ? count * 2 : 8;
    if (probe_display == display && probe_window != None &&
        atomic_load(&probe_presents) >= present_limit &&
        !atomic_exchange(&close_event_sent, 1)) {
        XEvent event = {0};
        event.xclient.type = ClientMessage;
        event.xclient.display = display;
        event.xclient.window = probe_window;
        event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", False);
        event.xclient.data.l[1] = CurrentTime;
        XSendEvent(display, probe_window, False, NoEventMask, &event);
        XFlush(display);
    }
    return real_function(display);
}

__attribute__((destructor)) static void report_probe_result(void) {
    uint32_t image_count = atomic_load(&probe_image_count);
    uint32_t acquisitions = atomic_load(&probe_acquisitions);
    uint32_t presents = atomic_load(&probe_presents);
    int nonsequential = atomic_load(&probe_nonsequential);
    int success = image_count > 0 && acquisitions > 0 && nonsequential &&
                  presents >= (image_count > 4 ? image_count * 2 : 8) &&
                  !atomic_load(&probe_failure);
    fprintf(stderr,
            "SWAPCHAIN_ACQUISITION_PROBE: success=%d presents=%u acquisitions=%u "
            "nonsequential=%d image_count=%u\n",
            success, presents, acquisitions, nonsequential, image_count);
}
