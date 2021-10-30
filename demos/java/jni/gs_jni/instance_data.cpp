#include "instance_data.h"

#include <unordered_map>

#include <assert.h>

static std::unordered_map<void *, GSInstanceData *> g_inst2Data; // instance -> data
static std::unordered_map<void *, GSInstanceData *> g_hand2Data; // caller handle -> data

GSInstanceData *putInstanceData(GSInstanceData *data)
{
	assert(g_inst2Data.find(data->instance) == g_inst2Data.end() && g_hand2Data.find(data->callerHandle) == g_hand2Data.end());

	g_inst2Data[data->instance] = data;
	g_hand2Data[data->callerHandle] = data;

	return data;
}

GSInstanceData *findDataFromInstance(void *instance)
{
	auto it = g_inst2Data.find(instance);
	return it == g_inst2Data.end() ? NULL : it->second;
}

GSInstanceData *findDataFromHandle(void *callerHandle)
{
	auto it = g_hand2Data.find(callerHandle);
	return it == g_hand2Data.end() ? NULL : it->second;
}

void deleteDataFromInstance(void *instance)
{
	auto i2dit = g_inst2Data.find(instance);
	if (i2dit != g_inst2Data.end())
	{
		GSInstanceData *idata = i2dit->second;
		auto h2dit = g_hand2Data.find(idata->callerHandle);

		assert(h2dit != g_hand2Data.end());

		g_inst2Data.erase(i2dit);
		g_hand2Data.erase(h2dit);
	}
}

void deleteDataFromHandle(void *callerHandle)
{
	auto h2dit = g_hand2Data.find(callerHandle);
	if (h2dit != g_hand2Data.end())
	{
		GSInstanceData *idata = h2dit->second;
		auto i2dit = g_inst2Data.find(idata->instance);

		assert(i2dit != g_inst2Data.end());

		g_hand2Data.erase(i2dit);
		g_inst2Data.erase(h2dit);
	}
}
