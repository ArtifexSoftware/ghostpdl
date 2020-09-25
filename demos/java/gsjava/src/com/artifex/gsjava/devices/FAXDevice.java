package com.artifex.gsjava.devices;

import com.artifex.gsjava.GSAPI;

public class FAXDevice extends FileDevice {

	public static final String FAXG3 = "faxg3";
	public static final String FAXG32D = "faxg32d";
	public static final String FAXG4 = "faxg4";

	public FAXDevice(String device) throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super(device);
	}

	public void setMinFeatureSize(int value) {
		setParam("MinFeatureSize", value, GSAPI.GS_SPT_INT);
	}
}
