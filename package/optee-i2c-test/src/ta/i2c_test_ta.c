// SPDX-License-Identifier: BSD-2-Clause
/*
 * Test TA for the OP-TEE I2C device drivers: forwards the client request
 * to the i2c_devices PTA, which only accepts calls from TAs.
 */
#include <i2c_test_ta.h>
#include <pta_i2c_devices.h>
#include <string.h>
#include <tee_internal_api.h>

static TEE_TASessionHandle pta_sess = TEE_HANDLE_NULL;

TEE_Result TA_CreateEntryPoint(void)
{
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t pt __unused,
				    TEE_Param params[4] __unused,
				    void **sess_ctx __unused)
{
	const TEE_UUID uuid = PTA_I2C_DEVICES_UUID;

	return TEE_OpenTASession(&uuid, TEE_TIMEOUT_INFINITE, 0, NULL,
				 &pta_sess, NULL);
}

void TA_CloseSessionEntryPoint(void *sess_ctx __unused)
{
	TEE_CloseTASession(pta_sess);
	pta_sess = TEE_HANDLE_NULL;
}

static bool is_memref(uint32_t type)
{
	return type == TEE_PARAM_TYPE_MEMREF_INPUT ||
	       type == TEE_PARAM_TYPE_MEMREF_OUTPUT ||
	       type == TEE_PARAM_TYPE_MEMREF_INOUT;
}

TEE_Result TA_InvokeCommandEntryPoint(void *sess_ctx __unused, uint32_t cmd,
				      uint32_t pt, TEE_Param params[4])
{
	TEE_Param fwd[TEE_NUM_PARAMS] = { };
	TEE_Result res = TEE_ERROR_GENERIC;
	void *bufs[TEE_NUM_PARAMS] = { };
	uint32_t type = 0;
	size_t n = 0;

	/*
	 * Bounce client memrefs through TA private memory: the PTA must not
	 * work on buffers the normal world can change underneath it.
	 */
	for (n = 0; n < TEE_NUM_PARAMS; n++) {
		type = TEE_PARAM_TYPE_GET(pt, n);
		fwd[n] = params[n];
		if (!is_memref(type) || !params[n].memref.size)
			continue;

		if (params[n].memref.size > TA_I2C_TEST_MAX_BUF) {
			res = TEE_ERROR_BAD_PARAMETERS;
			goto out;
		}

		bufs[n] = TEE_Malloc(params[n].memref.size, 0);
		if (!bufs[n]) {
			res = TEE_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		if (type != TEE_PARAM_TYPE_MEMREF_OUTPUT)
			memcpy(bufs[n], params[n].memref.buffer,
			       params[n].memref.size);
		fwd[n].memref.buffer = bufs[n];
	}

	res = TEE_InvokeTACommand(pta_sess, TEE_TIMEOUT_INFINITE, cmd, pt, fwd,
				  NULL);

	for (n = 0; n < TEE_NUM_PARAMS; n++) {
		type = TEE_PARAM_TYPE_GET(pt, n);
		if (is_memref(type)) {
			/* Size is updated on success and on short buffer */
			if (bufs[n] && type != TEE_PARAM_TYPE_MEMREF_INPUT &&
			    fwd[n].memref.size <= params[n].memref.size)
				memcpy(params[n].memref.buffer, bufs[n],
				       fwd[n].memref.size);
			params[n].memref.size = fwd[n].memref.size;
		} else if (type == TEE_PARAM_TYPE_VALUE_OUTPUT ||
			   type == TEE_PARAM_TYPE_VALUE_INOUT) {
			params[n].value = fwd[n].value;
		}
	}

out:
	for (n = 0; n < TEE_NUM_PARAMS; n++)
		TEE_Free(bufs[n]);

	return res;
}
