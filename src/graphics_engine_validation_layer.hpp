/*
	Due to the limited amount of handholding vulkan provides, we would like 
	a layer of validation between our API calls and the vulkan library
*/

#pragma once

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

bool checkValidationLayerSupport();