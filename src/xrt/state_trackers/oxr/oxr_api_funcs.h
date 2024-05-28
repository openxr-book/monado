// Copyright 2018-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Header defining all API functions.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup oxr_api
 */

#pragma once

#include "oxr_extension_support.h"

#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @defgroup oxr_api OpenXR entrypoints
 *
 * Gets called from the client application, does most verification and routes
 * calls into @ref oxr_main functions.
 *
 * @ingroup oxr
 * @{
 */


/*
 *
 * oxr_api_negotiate.c
 *
 */

//! OpenXR API function @ep{xrGetInstanceProcAddr}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetInstanceProcAddr(XrInstance instance, const char *name, PFN_xrVoidFunction *function);

//! OpenXR API function @ep{xrEnumerateApiLayerProperties}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateApiLayerProperties(uint32_t propertyCapacityInput,
                                  uint32_t *propertyCountOutput,
                                  XrApiLayerProperties *properties);


/*
 *
 * oxr_api_instance.c
 *
 */

#ifdef OXR_HAVE_KHR_loader_init
//! OpenXR API function @ep{xrInitializeLoaderKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrInitializeLoaderKHR(const XrLoaderInitInfoBaseHeaderKHR *loaderInitInfo);
#endif // OXR_HAVE_KHR_loader_init

//! OpenXR API function @ep{xrEnumerateInstanceExtensionProperties}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateInstanceExtensionProperties(const char *layerName,
                                           uint32_t propertyCapacityInput,
                                           uint32_t *propertyCountOutput,
                                           XrExtensionProperties *properties);

//! OpenXR API function @ep{xrCreateInstance}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateInstance(const XrInstanceCreateInfo *createInfo, XrInstance *instance);

//! OpenXR API function @ep{xrDestroyInstance}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyInstance(XrInstance instance);

//! OpenXR API function @ep{xrGetInstanceProperties}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetInstanceProperties(XrInstance instance, XrInstanceProperties *instanceProperties);

//! OpenXR API function @ep{xrPollEvent}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrPollEvent(XrInstance instance, XrEventDataBuffer *eventData);

//! OpenXR API function @ep{xrResultToString}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrResultToString(XrInstance instance, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE]);

//! OpenXR API function @ep{xrStructureTypeToString}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrStructureTypeToString(XrInstance instance, XrStructureType value, char buffer[XR_MAX_STRUCTURE_NAME_SIZE]);

//! OpenXR API function @ep{xrStringToPath}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrStringToPath(XrInstance instance, const char *pathString, XrPath *path);

//! OpenXR API function @ep{xrPathToString}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrPathToString(
    XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t *bufferCountOutput, char *buffer);

//! OpenXR API function @ep{xrConvertTimespecTimeToTimeKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrConvertTimespecTimeToTimeKHR(XrInstance instance, const struct timespec *timespecTime, XrTime *time);

//! OpenXR API function @ep{xrConvertTimeToTimespecTimeKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrConvertTimeToTimespecTimeKHR(XrInstance instance, XrTime time, struct timespec *timespecTime);

#ifdef XR_USE_PLATFORM_WIN32
//! OpenXR API function @ep{xrConvertWin32PerformanceCounterToTimeKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrConvertWin32PerformanceCounterToTimeKHR(XrInstance instance,
                                              const LARGE_INTEGER *performanceCounter,
                                              XrTime *time);

//! OpenXR API function @ep{xrConvertTimeToWin32PerformanceCounterKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrConvertTimeToWin32PerformanceCounterKHR(XrInstance instance, XrTime time, LARGE_INTEGER *performanceCounter);
#endif // XR_USE_PLATFORM_WIN32

/*
 *
 * oxr_api_system.c
 *
 */

//! OpenXR API function @ep{xrGetSystem}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetSystem(XrInstance instance, const XrSystemGetInfo *getInfo, XrSystemId *systemId);

//! OpenXR API function @ep{xrGetSystemProperties}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetSystemProperties(XrInstance instance, XrSystemId systemId, XrSystemProperties *properties);

//! OpenXR API function @ep{xrEnumerateViewConfigurations}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateViewConfigurations(XrInstance instance,
                                  XrSystemId systemId,
                                  uint32_t viewConfigurationTypeCapacityInput,
                                  uint32_t *viewConfigurationTypeCountOutput,
                                  XrViewConfigurationType *viewConfigurationTypes);

//! OpenXR API function @ep{xrGetViewConfigurationProperties}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetViewConfigurationProperties(XrInstance instance,
                                     XrSystemId systemId,
                                     XrViewConfigurationType viewConfigurationType,
                                     XrViewConfigurationProperties *configurationProperties);

//! OpenXR API function @ep{xrEnumerateViewConfigurationViews}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateViewConfigurationViews(XrInstance instance,
                                      XrSystemId systemId,
                                      XrViewConfigurationType viewConfigurationType,
                                      uint32_t viewCapacityInput,
                                      uint32_t *viewCountOutput,
                                      XrViewConfigurationView *views);

//! OpenXR API function @ep{xrEnumerateEnvironmentBlendModes}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateEnvironmentBlendModes(XrInstance instance,
                                     XrSystemId systemId,
                                     XrViewConfigurationType viewConfigurationType,
                                     uint32_t environmentBlendModeCapacityInput,
                                     uint32_t *environmentBlendModeCountOutput,
                                     XrEnvironmentBlendMode *environmentBlendModes);

#ifdef XR_USE_GRAPHICS_API_OPENGL
//! OpenXR API function @ep{xrGetOpenGLGraphicsRequirementsKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetOpenGLGraphicsRequirementsKHR(XrInstance instance,
                                       XrSystemId systemId,
                                       XrGraphicsRequirementsOpenGLKHR *graphicsRequirements);
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
//! OpenXR API function @ep{xrGetOpenGLESGraphicsRequirementsKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetOpenGLESGraphicsRequirementsKHR(XrInstance instance,
                                         XrSystemId systemId,
                                         XrGraphicsRequirementsOpenGLESKHR *graphicsRequirements);
#endif

#ifdef XR_USE_GRAPHICS_API_VULKAN
//! OpenXR API function @ep{xrGetVulkanInstanceExtensionsKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetVulkanInstanceExtensionsKHR(XrInstance instance,
                                     XrSystemId systemId,
                                     uint32_t namesCapacityInput,
                                     uint32_t *namesCountOutput,
                                     char *namesString);

//! OpenXR API function @ep{xrGetVulkanDeviceExtensionsKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetVulkanDeviceExtensionsKHR(XrInstance instance,
                                   XrSystemId systemId,
                                   uint32_t namesCapacityInput,
                                   uint32_t *namesCountOutput,
                                   char *namesString);

//! OpenXR API function @ep{xrGetVulkanGraphicsDeviceKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetVulkanGraphicsDeviceKHR(XrInstance instance,
                                 XrSystemId systemId,
                                 VkInstance vkInstance,
                                 VkPhysicalDevice *vkPhysicalDevice);

//! OpenXR API function @ep{xrGetVulkanGraphicsDeviceKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetVulkanGraphicsDevice2KHR(XrInstance instance,
                                  const XrVulkanGraphicsDeviceGetInfoKHR *getInfo,
                                  VkPhysicalDevice *vkPhysicalDevice);

//! OpenXR API function @ep{xrGetVulkanGraphicsRequirementsKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetVulkanGraphicsRequirementsKHR(XrInstance instance,
                                       XrSystemId systemId,
                                       XrGraphicsRequirementsVulkanKHR *graphicsRequirements);

//! OpenXR API function @ep{xrGetVulkanGraphicsRequirements2KHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetVulkanGraphicsRequirements2KHR(XrInstance instance,
                                        XrSystemId systemId,
                                        XrGraphicsRequirementsVulkan2KHR *graphicsRequirements);

//! OpenXR API function @ep{xrCreateVulkanInstanceKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateVulkanInstanceKHR(XrInstance instance,
                              const XrVulkanInstanceCreateInfoKHR *createInfo,
                              VkInstance *vulkanInstance,
                              VkResult *vulkanResult);

//! OpenXR API function @ep{xrCreateVulkanDeviceKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateVulkanDeviceKHR(XrInstance instance,
                            const XrVulkanDeviceCreateInfoKHR *createInfo,
                            VkDevice *vulkanDevice,
                            VkResult *vulkanResult);
#endif

#ifdef XR_USE_GRAPHICS_API_D3D11

//! OpenXR API function @ep{xrGetD3D11GraphicsRequirementsKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetD3D11GraphicsRequirementsKHR(XrInstance instance,
                                      XrSystemId systemId,
                                      XrGraphicsRequirementsD3D11KHR *graphicsRequirements);
#endif // XR_USE_GRAPHICS_API_D3D11

#ifdef XR_USE_GRAPHICS_API_D3D12

//! OpenXR API function @ep{xrGetD3D11GraphicsRequirementsKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetD3D12GraphicsRequirementsKHR(XrInstance instance,
                                      XrSystemId systemId,
                                      XrGraphicsRequirementsD3D12KHR *graphicsRequirements);
#endif // XR_USE_GRAPHICS_API_D3D12

/*
 *
 * oxr_api_session.c
 *
 */

//! OpenXR API function @ep{xrCreateSession}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateSession(XrInstance instance, const XrSessionCreateInfo *createInfo, XrSession *session);

//! OpenXR API function @ep{xrDestroySession}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroySession(XrSession session);

//! OpenXR API function @ep{xrBeginSession}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrBeginSession(XrSession session, const XrSessionBeginInfo *beginInfo);

//! OpenXR API function @ep{xrEndSession}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEndSession(XrSession session);

//! OpenXR API function @ep{xrWaitFrame}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrWaitFrame(XrSession session, const XrFrameWaitInfo *frameWaitInfo, XrFrameState *frameState);

//! OpenXR API function @ep{xrBeginFrame}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrBeginFrame(XrSession session, const XrFrameBeginInfo *frameBeginInfo);

//! OpenXR API function @ep{xrEndFrame}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEndFrame(XrSession session, const XrFrameEndInfo *frameEndInfo);

//! OpenXR API function @ep{xrRequestExitSession}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrRequestExitSession(XrSession session);

//! OpenXR API function @ep{xrLocateViews}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrLocateViews(XrSession session,
                  const XrViewLocateInfo *viewLocateInfo,
                  XrViewState *viewState,
                  uint32_t viewCapacityInput,
                  uint32_t *viewCountOutput,
                  XrView *views);

#ifdef OXR_HAVE_KHR_visibility_mask
//! OpenXR API function @ep{xrGetVisibilityMaskKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetVisibilityMaskKHR(XrSession session,
                           XrViewConfigurationType viewConfigurationType,
                           uint32_t viewIndex,
                           XrVisibilityMaskTypeKHR visibilityMaskType,
                           XrVisibilityMaskKHR *visibilityMask);
#endif // OXR_HAVE_KHR_visibility_mask

#ifdef OXR_HAVE_KHR_android_thread_settings
//! OpenXR API function @ep{xrSetAndroidApplicationThreadKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrSetAndroidApplicationThreadKHR(XrSession session, XrAndroidThreadTypeKHR threadType, uint32_t threadId);
#endif // OXR_HAVE_KHR_android_thread_settings

#ifdef OXR_HAVE_EXT_performance_settings
//! OpenXR API function @ep{xrPerfSettingsSetPerformanceLevelEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrPerfSettingsSetPerformanceLevelEXT(XrSession session,
                                         XrPerfSettingsDomainEXT domain,
                                         XrPerfSettingsLevelEXT level);
#endif // OXR_HAVE_EXT_performance_settings

#ifdef OXR_HAVE_EXT_thermal_query
//! OpenXR API function @ep{xrThermalGetTemperatureTrendEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrThermalGetTemperatureTrendEXT(XrSession session,
                                    XrPerfSettingsDomainEXT domain,
                                    XrPerfSettingsNotificationLevelEXT *notificationLevel,
                                    float *tempHeadroom,
                                    float *tempSlope);
#endif // OXR_HAVE_EXT_thermal_query


/*
 *
 * oxr_api_space.c
 *
 */

//! OpenXR API function @ep{xrEnumerateReferenceSpaces}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateReferenceSpaces(XrSession session,
                               uint32_t spaceCapacityInput,
                               uint32_t *spaceCountOutput,
                               XrReferenceSpaceType *spaces);

//! OpenXR API function @ep{xrGetReferenceSpaceBoundsRect}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetReferenceSpaceBoundsRect(XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df *bounds);

//! OpenXR API function @ep{xrCreateReferenceSpace}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo *createInfo, XrSpace *space);

//! OpenXR API function @ep{xrLocateSpace}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation *location);

//! OpenXR API function @ep{xrDestroySpace}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroySpace(XrSpace space);


/*
 *
 * oxr_api_swapchain.c
 *
 */

//! OpenXR API function @ep{xrEnumerateSwapchainFormats}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateSwapchainFormats(XrSession session,
                                uint32_t formatCapacityInput,
                                uint32_t *formatCountOutput,
                                int64_t *formats);

//! OpenXR API function @ep{xrCreateSwapchain}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo *createInfo, XrSwapchain *swapchain);

//! OpenXR API function @ep{xrDestroySwapchain}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroySwapchain(XrSwapchain swapchain);

//! OpenXR API function @ep{xrEnumerateSwapchainImages}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateSwapchainImages(XrSwapchain swapchain,
                               uint32_t imageCapacityInput,
                               uint32_t *imageCountOutput,
                               XrSwapchainImageBaseHeader *images);

//! OpenXR API function @ep{xrAcquireSwapchainImage}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo *acquireInfo, uint32_t *index);

//! OpenXR API function @ep{xrWaitSwapchainImage}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo *waitInfo);

//! OpenXR API function @ep{xrReleaseSwapchainImage}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo *releaseInfo);


/*
 *
 * oxr_api_debug.c
 *
 */

//! OpenXR API function @ep{xrSetDebugUtilsObjectNameEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrSetDebugUtilsObjectNameEXT(XrInstance instance, const XrDebugUtilsObjectNameInfoEXT *nameInfo);

//! OpenXR API function @ep{xrCreateDebugUtilsMessengerEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateDebugUtilsMessengerEXT(XrInstance instance,
                                   const XrDebugUtilsMessengerCreateInfoEXT *createInfo,
                                   XrDebugUtilsMessengerEXT *messenger);

//! OpenXR API function @ep{xrDestroyDebugUtilsMessengerEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyDebugUtilsMessengerEXT(XrDebugUtilsMessengerEXT messenger);

//! OpenXR API function @ep{xrSubmitDebugUtilsMessageEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrSubmitDebugUtilsMessageEXT(XrInstance instance,
                                 XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                 XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                                 const XrDebugUtilsMessengerCallbackDataEXT *callbackData);

//! OpenXR API function @ep{xrSessionBeginDebugUtilsLabelRegionEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrSessionBeginDebugUtilsLabelRegionEXT(XrSession session, const XrDebugUtilsLabelEXT *labelInfo);

//! OpenXR API function @ep{xrSessionEndDebugUtilsLabelRegionEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrSessionEndDebugUtilsLabelRegionEXT(XrSession session);

//! OpenXR API function @ep{xrSessionInsertDebugUtilsLabelEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrSessionInsertDebugUtilsLabelEXT(XrSession session, const XrDebugUtilsLabelEXT *labelInfo);


/*
 *
 * oxr_api_action.c
 *
 */

//! OpenXR API function @ep{xrCreateActionSpace}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo *createInfo, XrSpace *space);

//! OpenXR API function @ep{xrCreateActionSet}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo *createInfo, XrActionSet *actionSet);

//! OpenXR API function @ep{xrDestroyActionSet}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyActionSet(XrActionSet actionSet);

//! OpenXR API function @ep{xrCreateAction}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo *createInfo, XrAction *action);

//! OpenXR API function @ep{xrDestroyAction}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyAction(XrAction action);

//! OpenXR API function @ep{xrSuggestInteractionProfileBindings}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrSuggestInteractionProfileBindings(XrInstance instance,
                                        const XrInteractionProfileSuggestedBinding *suggestedBindings);

//! OpenXR API function @ep{xrAttachSessionActionSets}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo *bindInfo);

//! OpenXR API function @ep{xrGetCurrentInteractionProfile}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetCurrentInteractionProfile(XrSession session,
                                   XrPath topLevelUserPath,
                                   XrInteractionProfileState *interactionProfile);

//! OpenXR API function @ep{xrGetActionStateBoolean}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo *getInfo, XrActionStateBoolean *data);

//! OpenXR API function @ep{xrGetActionStateFloat}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo *getInfo, XrActionStateFloat *data);

//! OpenXR API function @ep{xrGetActionStateVector2f}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo *getInfo, XrActionStateVector2f *data);

//! OpenXR API function @ep{xrGetActionStatePose}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetActionStatePose(XrSession session, const XrActionStateGetInfo *getInfo, XrActionStatePose *data);

//! OpenXR API function @ep{xrSyncActions}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrSyncActions(XrSession session, const XrActionsSyncInfo *syncInfo);

//! OpenXR API function @ep{xrEnumerateBoundSourcesForAction}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateBoundSourcesForAction(XrSession session,
                                     const XrBoundSourcesForActionEnumerateInfo *enumerateInfo,
                                     uint32_t sourceCapacityInput,
                                     uint32_t *sourceCountOutput,
                                     XrPath *sources);

//! OpenXR API function @ep{xrGetInputSourceLocalizedName}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetInputSourceLocalizedName(XrSession session,
                                  const XrInputSourceLocalizedNameGetInfo *getInfo,
                                  uint32_t bufferCapacityInput,
                                  uint32_t *bufferCountOutput,
                                  char *buffer);

//! OpenXR API function @ep{xrApplyHapticFeedback}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrApplyHapticFeedback(XrSession session,
                          const XrHapticActionInfo *hapticActionInfo,
                          const XrHapticBaseHeader *hapticEvent);

//! OpenXR API function @ep{xrStopHapticFeedback}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrStopHapticFeedback(XrSession session, const XrHapticActionInfo *hapticActionInfo);

//! OpenXR API function @ep{xrCreateHandTrackerEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateHandTrackerEXT(XrSession session,
                           const XrHandTrackerCreateInfoEXT *createInfo,
                           XrHandTrackerEXT *handTracker);

//! OpenXR API function @ep{xrDestroyHandTrackerEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyHandTrackerEXT(XrHandTrackerEXT handTracker);

//! OpenXR API function @ep{xrLocateHandJointsEXT}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrLocateHandJointsEXT(XrHandTrackerEXT handTracker,
                          const XrHandJointsLocateInfoEXT *locateInfo,
                          XrHandJointLocationsEXT *locations);

//! OpenXR API function @ep{xrApplyForceFeedbackCurlMNDX}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrApplyForceFeedbackCurlMNDX(XrHandTrackerEXT handTracker, const XrForceFeedbackCurlApplyLocationsMNDX *locations);


//! OpenXR API function @ep{xrEnumerateDisplayRefreshRatesFB}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateDisplayRefreshRatesFB(XrSession session,
                                     uint32_t displayRefreshRateCapacityInput,
                                     uint32_t *displayRefreshRateCountOutput,
                                     float *displayRefreshRates);

//! OpenXR API function @ep{xrGetDisplayRefreshRateFB}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetDisplayRefreshRateFB(XrSession session, float *displayRefreshRate);

//! OpenXR API function @ep{xrRequestDisplayRefreshRateFB}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrRequestDisplayRefreshRateFB(XrSession session, float displayRefreshRate);


//! OpenXR API function @ep{xrLocateSpacesKHR}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrLocateSpacesKHR(XrSession session, const XrSpacesLocateInfoKHR *locateInfo, XrSpaceLocationsKHR *spaceLocations);

//! OpenXR API function @ep{xrLocateSpaces}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrLocateSpaces(XrSession session, const XrSpacesLocateInfo *locateInfo, XrSpaceLocations *spaceLocations);

/*
 *
 * oxr_api_passthrough.c
 *
 */
#ifdef OXR_HAVE_FB_passthrough
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateGeometryInstanceFB(XrSession session,
                               const XrGeometryInstanceCreateInfoFB *createInfo,
                               XrGeometryInstanceFB *outGeometryInstance);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreatePassthroughFB(XrSession session,
                          const XrPassthroughCreateInfoFB *createInfo,
                          XrPassthroughFB *outPassthrough);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreatePassthroughLayerFB(XrSession session,
                               const XrPassthroughLayerCreateInfoFB *createInfo,
                               XrPassthroughLayerFB *outLayer);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyGeometryInstanceFB(XrGeometryInstanceFB instance);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyPassthroughFB(XrPassthroughFB passthrough);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyPassthroughLayerFB(XrPassthroughLayerFB layer);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGeometryInstanceSetTransformFB(XrGeometryInstanceFB instance,
                                     const XrGeometryInstanceTransformFB *transformation);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrPassthroughLayerPauseFB(XrPassthroughLayerFB layer);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrPassthroughLayerResumeFB(XrPassthroughLayerFB layer);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrPassthroughLayerSetStyleFB(XrPassthroughLayerFB layer, const XrPassthroughStyleFB *style);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrPassthroughPauseFB(XrPassthroughFB passthrough);
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrPassthroughStartFB(XrPassthroughFB passthrough);
#endif

#ifdef OXR_HAVE_HTC_facial_tracking
//! OpenXR API function @ep{xrCreateFacialTrackerHTC}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateFacialTrackerHTC(XrSession session,
                             const XrFacialTrackerCreateInfoHTC *createInfo,
                             XrFacialTrackerHTC *facialTracker);

//! OpenXR API function @ep{xrDestroyFacialTrackerHTC}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyFacialTrackerHTC(XrFacialTrackerHTC facialTracker);

//! OpenXR API function @ep{xrGetFacialExpressionsHTC}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetFacialExpressionsHTC(XrFacialTrackerHTC facialTracker, XrFacialExpressionsHTC *facialExpressions);
#endif

#ifdef OXR_HAVE_FB_body_tracking
//! OpenXR API function @ep{xrCreateBodyTrackerFB}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateBodyTrackerFB(XrSession session, const XrBodyTrackerCreateInfoFB *createInfo, XrBodyTrackerFB *bodyTracker);

//! OpenXR API function @ep{xrDestroyBodyTrackerFB}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyBodyTrackerFB(XrBodyTrackerFB bodyTracker);

//! OpenXR API function @ep{xrGetBodySkeletonFB}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetBodySkeletonFB(XrBodyTrackerFB bodyTracker, XrBodySkeletonFB *skeleton);

//! OpenXR API function @ep{xrLocateBodyJointsFB}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrLocateBodyJointsFB(XrBodyTrackerFB bodyTracker,
                         const XrBodyJointsLocateInfoFB *locateInfo,
                         XrBodyJointLocationsFB *locations);
#endif


/*
 *
 * oxr_api_xdev.c
 *
 */

#ifdef OXR_HAVE_MNDX_xdev_space
//! OpenXR API function @ep{xrCreateXDevListMNDX}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateXDevListMNDX(XrSession session, const XrCreateXDevListInfoMNDX *info, XrXDevListMNDX *xdevList);

//! OpenXR API function @ep{xrGetXDevListGenerationNumberMNDX}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetXDevListGenerationNumberMNDX(XrXDevListMNDX session, uint64_t *outGeneration);

//! OpenXR API function @ep{xrEnumerateXDevsMNDX}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateXDevsMNDX(XrXDevListMNDX xdevList,
                         uint32_t xdevCapacityInput,
                         uint32_t *xdevCountOutput,
                         XrXDevIdMNDX *xdevs);

//! OpenXR API function @ep{xrGetXDevProperty}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetXDevPropertiesMNDX(XrXDevListMNDX xdevList, const XrGetXDevInfoMNDX *info, XrXDevPropertiesMNDX *properties);

//! OpenXR API function @ep{xrDestroyXDevListMNDX}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyXDevListMNDX(XrXDevListMNDX xdevList);

//! OpenXR API function @ep{xrCreateXDevSpace}
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateXDevSpaceMNDX(XrSession session, const XrCreateXDevSpaceInfoMNDX *createInfo, XrSpace *space);
#endif


/*!
 * @}
 */


#ifdef __cplusplus
}
#endif
