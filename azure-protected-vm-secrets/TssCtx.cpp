// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"
#include "TssCtx.h"
#include "TpmError.h"
#include "ReturnCodes.h"
#ifndef PLATFORM_UNIX
#include "tss2/tss2_tcti_tbs.h"  // Windows context handling is routed to tbs library
#define TPM_DEVICE "" // For windows we don't need the device Manager context string. 
#else 
#include "tss2/tss2_tcti_device.h"  // Windows context handling is routed to tbs library
#define TPM_DEVICE "/dev/tpmrm0" // Use in-kernel resource manager.
#endif // !PLATFORM_UNIX


TssCtx::TssCtx()
{
    TSS2_ABI_VERSION abiVer = TSS2_ABI_VERSION_CURRENT; // These are the current default values of the TPM2-TSS library.

    auto tcti = InitializeTcti();

    TSS2_RC ret = Esys_Initialize(&ctx, tcti, &abiVer);
    if (ret != TSS2_RC_SUCCESS) {
		// TpmError, Subclass Context, esysInitError
        throw TpmError(ret, "Failed to initialize TSS context",
            ErrorCode::TpmError_Context_esysInitError);
    }
}

TssCtx::~TssCtx()
{
    // Esys_Finalize will free its own memory for ctx. Tss2_Tcti_Finalize will not,
    // but its memory is managed by a unique_ptr.
    if (ctx != nullptr) {
        Esys_Finalize(&ctx);
    }

    if (tctiCtx != nullptr) {
        Tss2_Tcti_Finalize((TSS2_TCTI_CONTEXT*)tctiCtx.get());
    }
}

ESYS_CONTEXT* TssCtx::Get()
{
    return this->ctx;
}

/**
 * Initializes TCTI interface. Uses a direct connection to the tpm resource
 * resource manager device file.
 */
TSS2_TCTI_CONTEXT* TssCtx::InitializeTcti()
{
    TSS2_RC ret{ TSS2_TCTI_RC_GENERAL_FAILURE };
    size_t size{ 0 };
    const char* device = TPM_DEVICE;
    // Get tcti size
#ifdef PLATFORM_UNIX
    ret = Tss2_Tcti_Device_Init(nullptr, &size, nullptr);
#else
    ret = Tss2_Tcti_Tbs_Init(nullptr, &size, nullptr);
#endif // PLATFORM_UNIX
    if (ret != TSS2_RC_SUCCESS) {
		// TpmError, Subclass Context, tctiInitError
        throw TpmError(ret, "Failed to initialize TSS context - size",
            ErrorCode::TpmError_Context_tctiInitError);
    }

    tctiCtx = std::make_unique<unsigned char[]>(size);
    if (tctiCtx == nullptr) {
		// GeneralError, MemoryError, allocationError
        throw std::runtime_error("Failed to allocate TCTI context memory");
    }

    // Populate TCTI context
#ifdef PLATFORM_UNIX
    ret = Tss2_Tcti_Device_Init((TSS2_TCTI_CONTEXT*)tctiCtx.get(), &size, device);
#else
    ret = Tss2_Tcti_Tbs_Init((TSS2_TCTI_CONTEXT*)tctiCtx.get(), &size, device);
#endif // PLATFORM_UNIX

    if (ret != TSS2_RC_SUCCESS) {
        // TpmError, Subclass Context, tctiInitError
        throw TpmError(ret, "Failed to initialize TCTI context",
            ErrorCode::TpmError_Context_tctiInitError);
    }

    return (TSS2_TCTI_CONTEXT*)tctiCtx.get();
}