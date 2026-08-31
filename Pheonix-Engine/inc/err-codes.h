#pragma once

typedef enum {
    ERR_SUCCESS = 0, // General Errors
    ERR_FAILURE,
    ERR_UNKNOWN,

    ERR_USAGE = 0x1000, // Basic Errors
    ERR_INTERNAL,

    ERR_MAGIC_INVALID = 0x2000, // Verification Errors
    ERR_VERSION_INVALID,

    ERR_ALLOC_FAILED = 0x3000, // Memory Errors

    ERR_COULD_NOT_OPEN_FILE = 0x4000, // File Errors

    ERR_WS_UNSUPPORTED = 0x5000, // Window System Errors
    ERR_WS_INIT_FAILED,
    ERR_WS_UNINITIALIZED,
    ERR_WS_NO_WINDOW_FOUND,

	ERR_RS_INVALID_BACKEND = 0x6000, // Rendering System Errors

    ERR_GL_PROGRAM_CREATION_FAILED = 0x6200, // Rendering System - OpenGL Errors
    ERR_GL_GLEW_INIT_FAILED,

	ERR_VK_LIB_LOAD_FAILED = 0x6400, // Rendering System - Vulkan Errors
	ERR_VK_EXTENSION_ENUMERATION_FAILED,
	ERR_VK_LAYER_ENUMERATION_FAILED
} t_err_codes;
