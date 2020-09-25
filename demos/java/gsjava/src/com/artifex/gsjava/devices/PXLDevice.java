package com.artifex.gsjava.devices;

import com.artifex.gsjava.GSAPI;

public class PXLDevice extends FileDevice implements HighLevelDevice {

	public static final String PXLMONO = "pxlmono";
	public static final String PXLCOLOR = "pxlcolor";

	public PXLDevice(String device) throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super(device);
	}

	public void setCompressMode(int mode) {
		setParam("CompressMode", mode, GSAPI.GS_SPT_INT);
	}
}
