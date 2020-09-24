package com.artifex.gsjava.devices;

import com.artifex.gsjava.GSAPI;

public class JPEGDevice extends FileDevice {

	public JPEGDevice()
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super("jpeg");
	}

	public void setJPEGQuality(int quality) {
		setParam("JPEGQ", quality, GSAPI.GS_SPT_INT);
	}

	public void setQualityFactor(float scale) {
		setParam("QFactor", scale, GSAPI.GS_SPT_FLOAT);
	}
}
