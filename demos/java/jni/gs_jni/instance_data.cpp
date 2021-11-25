#include "instance_data.h"

#include <unordered_map>
#include <assert.h>
#include <mutex>

#if !defined(GSJNI_NO_MT)

static std::unordered_map<void *, GSInstanceData *> g_inst2Data; // instance -> data
static std::mutex g_mtx;

#else

static GSInstanceData g_global; // Global instance for faster multithreading

#endif

GSInstanceData *putInstanceData(GSInstanceData *data)
{
#if !defined(GSJNI_NO_MT)
	g_mtx.lock();

	assert(g_inst2Data.find(data->instance) == g_inst2Data.end());

	g_inst2Data[data->instance] = data;

	g_mtx.unlock();

	return data;

#else
	delete data;
	return &g_global;
#endif
}

GSInstanceData *findDataFromInstance(void *instance)
{
#if !defined(GSJNI_NO_MT)
	g_mtx.lock();

	auto it = g_inst2Data.find(instance);
	GSInstanceData *result = it == g_inst2Data.end() ? NULL : it->second;

	g_mtx.unlock();

	return result;
#else
	return &g_global;
#endif
}

void deleteDataFromInstance(void *instance)
{
#if !defined(GSJNI_NO_MT)
	g_mtx.lock();

	auto i2dit = g_inst2Data.find(instance);
	if (i2dit != g_inst2Data.end())
		g_inst2Data.erase(i2dit);

	g_mtx.unlock();
#endif
}