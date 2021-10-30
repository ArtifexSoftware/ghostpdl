#include "instance_data.h"

#include <unordered_map>

#include <assert.h>

static GSInstanceData g_global; // Global instance for faster multithreading

#if defined(ALLOW_MT)

static std::unordered_map<void *, GSInstanceData *> g_inst2Data; // instance -> data
static std::unordered_map<void *, GSInstanceData *> g_hand2Data; // caller handle -> data

#endif

GSInstanceData *putInstanceData(GSInstanceData *data)
{
#if defined(ALLOW_MT)
	assert(g_inst2Data.find(data->instance) == g_inst2Data.end() && g_hand2Data.find(data->callerHandle) == g_hand2Data.end());

	g_inst2Data[data->instance] = data;
	g_hand2Data[data->callerHandle] = data;

	return data;

#else
	delete data;
	return &g_global;
#endif
}

GSInstanceData *findDataFromInstance(void *instance)
{
#if defined(ALLOW_MT)
	auto it = g_inst2Data.find(instance);
	return it == g_inst2Data.end() ? NULL : it->second;
#else
	return &g_global;
#endif
}

GSInstanceData *findDataFromHandle(void *callerHandle)
{
#if defined(ALLOW_MT)
	auto it = g_hand2Data.find(callerHandle);
	return it == g_hand2Data.end() ? NULL : it->second;
#else
	return &g_global;
#endif
}

void deleteDataFromInstance(void *instance)
{
#if defined(ALLOW_MT)
	auto i2dit = g_inst2Data.find(instance);
	if (i2dit != g_inst2Data.end())
	{
		GSInstanceData *idata = i2dit->second;
		auto h2dit = g_hand2Data.find(idata->callerHandle);

		assert(h2dit != g_hand2Data.end());

		g_inst2Data.erase(i2dit);
		g_hand2Data.erase(h2dit);
	}
#endif
}

void deleteDataFromHandle(void *callerHandle)
{
#if defined(ALLOW_MT)
	auto h2dit = g_hand2Data.find(callerHandle);
	if (h2dit != g_hand2Data.end())
	{
		GSInstanceData *idata = h2dit->second;
		auto i2dit = g_inst2Data.find(idata->instance);

		assert(i2dit != g_inst2Data.end());

		g_hand2Data.erase(i2dit);
		g_inst2Data.erase(h2dit);
	}
#endif
}
